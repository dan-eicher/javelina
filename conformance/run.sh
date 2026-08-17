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

# ── deferral comments ───────────────────────────────────────────────────────
# A comment that DEFERS a cited §section must not coexist with a COVERED claim for it. "A
# later refinement" written in a source file is not a record, it is a place things go to be
# forgotten — this turns one into a ledger row.
#
# The only gate still in shell, and for a reason rather than by omission: it scans the
# COMPILER's and the VM's sources, which lie outside the tree the guest reaches through
# --root. The inventory and coverage gates run on javelina, further down, once there is a jre
# to run them against.
sh conformance/check-deferrals.sh

# The number of assertions conformance/jls declares. Same reason as EXPECT_KERNELS: a chapter
# that stops running, or a section quietly dropped from a run() list, changes this count, and
# that has to FAIL rather than shrink the corpus into a smaller green.
EXPECT_JLS_CHECKS=440

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
$B/javelinac --libdir $LIBDIR -O0 conformance/jls -o $B/conf-jls-O0.wasm
$B/javelinac --libdir $LIBDIR -O  conformance/jls -o $B/conf-jls-O.wasm

# ── the compiler's OWN memory, checked ──────────────────────────────────────
# A whole class of defect is invisible to every behavioural suite: the optimizer's
# DSE indexed mem_kind/mem_elem/mem_spine to vnode_count when they are sized
# mem_rows, and that over-read shipped green through 60113 wast execution cases,
# the compiler suite, the CLI suite and the four-config gate below — because
# reading past an arena array lands on zeroes. Nothing here can catch that; a
# memory checker can, and this is the one place the gate runs javelinac over a
# real non-RTL program.
#
# It must be VALGRIND, not ASAN, and test-exec-asan does not substitute: bbq_arena
# bump-allocates sub-ranges inside one big malloc'd block, so an over-read of a
# sub-array never crosses a redzone and ASAN sees nothing. Memcheck tracks
# definedness per byte, so it reports the uninitialised read — which is how this
# one was found. Costs ~6 min; that is the price of the only check that covers it.
#
# Skipped (loudly) when valgrind is absent, so the gate still runs on a machine
# without it — but never silently, because a check that quietly does nothing is
# indistinguishable from one that passes.
# LEAKS ARE GATED TOO, and that is not tidiness. javelinac reached `in use at exit:
# 0 bytes` only once release_compile started freeing the source path each parse ctx
# owns; before that it leaked one string per file and the heap was never emptied.
# A heap that is never emptied cannot be checked — a use-after-free is undetectable
# when nothing is released — so the zero IS the instrument, and an ungated zero is
# one nobody notices leaving. Definite only: `possibly lost` is interior pointers
# into the arenas and would make this a noise generator.
VG_LEAK="--leak-check=full --show-leak-kinds=definite"
if command -v valgrind > /dev/null 2>&1; then
    VGOUT=$(mktemp)
    # Memory ERRORS still fail outright — those are never acceptable. Definite
    # LEAKS are ratcheted against conformance/leak-floor: the standing count is
    # recorded there with its owners, and any rise is a new leak.
    valgrind --error-exitcode=9 --errors-for-leak-kinds=none $VG_LEAK \
             $B/javelinac --libdir $LIBDIR -O $SRC -o /dev/null > /dev/null 2> "$VGOUT"
    VGRC=$?
    LEAKS=$(grep -c "definitely lost in loss record" "$VGOUT" || true)
    FLOOR=$(grep -vE '^[[:space:]]*#|^[[:space:]]*$' conformance/leak-floor | tail -1)
    if [ "$VGRC" -ne 0 ]; then
        echo "  FAIL  javelinac -O under valgrind — MEMORY ERROR compiling conformance/src"
        grep -E "Invalid|Mismatched|uninitialised|Conditional" "$VGOUT" | sed 's/^/        | /' | head -30
        rm -f "$VGOUT"; echo "java e2e conformance: 0 passed, 1 failed"; exit 1
    elif [ "$LEAKS" -gt "$FLOOR" ]; then
        echo "  FAIL  javelinac leaks: $LEAKS definite loss record(s), ceiling $FLOOR"
        echo "        A NEW leak. Fix the lifetime; do not raise conformance/leak-floor."
        grep -A5 "definitely lost in loss record" "$VGOUT" | sed 's/^/        | /' | head -40
        rm -f "$VGOUT"; echo "java e2e conformance: 0 passed, 1 failed"; exit 1
    elif [ "$LEAKS" -lt "$FLOOR" ]; then
        echo "  FAIL  javelinac leaks: $LEAKS < ceiling $FLOOR — LOWER conformance/leak-floor to $LEAKS"
        echo "        A lifetime was fixed; record it, so the ceiling cannot drift up again."
        rm -f "$VGOUT"; echo "java e2e conformance: 0 passed, 1 failed"; exit 1
    else
        echo "  ....  javelinac -O under valgrind (the corpus, non-RTL; 0 errors, $LEAKS leak(s) at ceiling)"
    fi
    rm -f "$VGOUT"
else
    echo "  SKIP  valgrind not installed — the compiler's own memory is NOT checked"
fi

# ── the matrix ──────────────────────────────────────────────────────────────
if [ "$MODE" = quick ]; then ARG=quick; else ARG=full; fi

# -O0/-O is the COMPILER's level — which javelinac built this pair. t0/t1/t2 is the
# RUNTIME's engine, and that is --tier N; the spelling matches clbg.sh's. It was
# -nojit/-jit, two configs, when the runtime had two engines. Tier-2 is the third, and
# it belongs in this matrix rather than only in the .wast corpus: what the star-diff
# below judges is a real Java program's allocation, collection and class layout, which
# the wast assertions do not exercise at all. A tier that runs only where the corpus
# looks is a tier that drifts.
for cfg in O0-t0 O0-t1 O0-t2 O-t0 O-t1 O-t2; do
    case $cfg in
        O0-*) jre=conf-jre-O0.wasm; plug=conf-gct-O0.wasm ;;
        O-*)  jre=conf-jre-O.wasm;  plug=conf-gct-O.wasm  ;;
    esac
    tier="--tier ${cfg#*-t}"
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
awk '/^RESULT/' $B/conf-O0-t0.out > $B/conf-ref.chk

n=$(wc -l < $B/conf-ref.chk | tr -d ' ')
if [ "$n" -ne "$EXPECT_KERNELS" ]; then
    echo "  FAIL  gc-torture: $n RESULT lines, expected $EXPECT_KERNELS (seed $SEED)"
    echo "        a corpus that shrinks silently is the defect this check exists for"
    echo "java e2e conformance: 0 passed, 1 failed"
    exit 1
fi

for cfg in O0-t1 O0-t2 O-t0 O-t1 O-t2; do
    awk '/^RESULT/' $B/conf-$cfg.out > $B/conf-$cfg.chk
    if ! diff $B/conf-ref.chk $B/conf-$cfg.chk > /dev/null; then
        echo "  FAIL  gc-torture: O0-t0 and $cfg disagree — that config is WRONG, not slow"
        echo "        replay: sh conformance/run.sh --$MODE --seed $SEED"
        diff $B/conf-ref.chk $B/conf-$cfg.chk | sed 's/^/        | /' || true
        echo "java e2e conformance: 0 passed, 1 failed"
        exit 1
    fi
done

# ── the heap-invariant checker, over the corpus ─────────────────────────────
# The four configs above agree on a checksum; that is a strong oracle for VALUES
# and a weak one for the HEAP. A collector can leave a stale forwarding pointer,
# an unmarked live object or an inconsistent line map and still hand back every
# right answer for several collections — which is exactly what happened here:
# breaking gc_mark1's forwarding update changed no checksum anywhere in this
# corpus back when evacuation never ran.
#
# So the compiled configs run again with the collector checking its own invariants
# after every collection (--verify-heap). A violation stops the vm and surfaces as a
# trap naming the invariant, at the end of the cycle that broke it — the only point
# where the cause is still attributable. -O is the level: the most optimizer
# transformation, so a wrongly-dropped ArrayStore check or a bad memory-DSE lands here.
#
# BOTH JIT tiers run it. opgen's `sclass_cacheable` excludes references from the cache
# deliberately — a reference has to stay on the operand stack "where the collector
# looks for roots and, because it evacuates, where it rewrites them" — so the argument
# says t2 is no more exposed than t1. That argument is a comment, and a comment is not
# a run: this leg is cheap to point at t2 and is the only thing that would notice if a
# cached slot ever did hold a reference the collector could not find or rewrite.
#
# Their RESULT lines join the star-diff, so neither leg can pass by not running.
for vt in 1 2; do
    if $B/javelina --jre $B/conf-jre-O.wasm --tier $vt --verify-heap $B/conf-gct-O.wasm \
                   "$ARG" "$SEED" > $B/conf-verify-t$vt.out 2> $B/conf-verify-t$vt.err; then
        awk '/^RESULT/' $B/conf-verify-t$vt.out > $B/conf-verify-t$vt.chk
        if diff $B/conf-ref.chk $B/conf-verify-t$vt.chk > /dev/null; then
            echo "  ....  gc-torture O-t$vt under --verify-heap"
        else
            echo "  FAIL  gc-torture --verify-heap O-t$vt: results differ from O0-t0"
            diff $B/conf-ref.chk $B/conf-verify-t$vt.chk | sed 's/^/        | /' || true
            echo "java e2e conformance: 0 passed, 1 failed"
            exit 1
        fi
    else
        echo "  FAIL  gc-torture --verify-heap O-t$vt (exit $?, seed $SEED) — a heap invariant broke"
        sed 's/^/        | /' $B/conf-verify-t$vt.out
        sed 's/^/        | /' $B/conf-verify-t$vt.err
        echo "        replay: $B/javelina --jre $B/conf-jre-O.wasm --tier $vt --verify-heap \\"
        echo "                $B/conf-gct-O.wasm $ARG $SEED"
        echo "java e2e conformance: 0 passed, 1 failed"
        exit 1
    fi
done

echo "  PASS  gc-torture ($EXPECT_KERNELS kernels × 6 configs agree, seed $SEED, $MODE)"

# ── the JLS suite ───────────────────────────────────────────────────────────
# gc-torture is a differential oracle: it proves the four configs AGREE, which is a strong
# check on the engine and says nothing about whether the agreed answer is the one the
# language specifies. conformance/jls is the other half — every assertion cites the JLS
# section it comes from, so a failure names a RULE rather than a checksum.
#
# Run on all four configs for the same reason: a §5.1.3 narrowing that the interpreter gets
# right and the JIT gets wrong is a config being WRONG, and the count makes it visible.
for cfg in O0-t0 O0-t1 O0-t2 O-t0 O-t1 O-t2; do
    case $cfg in
        O0-*) jre=conf-jre-O0.wasm; plug=conf-jls-O0.wasm ;;
        O-*)  jre=conf-jre-O.wasm;  plug=conf-jls-O.wasm  ;;
    esac
    tier="--tier ${cfg#*-t}"                        # see the gc-torture matrix
    if ! $B/javelina --jre $B/$jre $tier $B/$plug > $B/jls-$cfg.out 2> $B/jls-$cfg.err; then
        echo "  FAIL  jls $cfg — a cited JLS rule does not hold (or the program died)"
        sed 's/^/        | /' $B/jls-$cfg.out
        sed 's/^/        | /' $B/jls-$cfg.err
        echo "java e2e conformance: 0 passed, 1 failed"
        exit 1
    fi
    # The RESULT line carries the count, and the count is asserted: `fails=0` alone passes
    # vacuously on a suite that ran nothing, which is the same defect EXPECT_KERNELS guards.
    got=$(sed -n 's/^RESULT jls checks=\([0-9]*\) fails=.*/\1/p' $B/jls-$cfg.out)
    bad=$(sed -n 's/^RESULT jls checks=[0-9]* fails=\([0-9]*\)/\1/p' $B/jls-$cfg.out)
    if [ "$got" != "$EXPECT_JLS_CHECKS" ] || [ "$bad" != "0" ]; then
        echo "  FAIL  jls $cfg: $got checks / $bad failures, expected $EXPECT_JLS_CHECKS / 0"
        grep '^FAIL' $B/jls-$cfg.out | sed 's/^/        | /'
        echo "java e2e conformance: 0 passed, 1 failed"
        exit 1
    fi
    echo "  ....  jls $cfg ($got checks)"
done
echo "  PASS  jls ($EXPECT_JLS_CHECKS cited-section checks × 6 configs)"

# ── the STITCHED corpus (P4) ────────────────────────────────────────────────
# The plan's deliverable gate: "every stitched program's actual stdout/exit code == its
# composed expectation, both tiers". Runs BEFORE the join below, because it is what writes
# conformance/generated — the directory the join scans for `// JLS <n>` markers. Skipping it
# would leave the join reading a stale or empty directory and reporting less coverage than
# exists, which is how 41 covered sections went uncounted until this leg was wired up.
sh conformance/run-generated.sh

# ── the inventory and coverage gates, ON the VM ─────────────────────────────
# Transcription invariants, status discipline, the join in both directions, the ratchet, and
# the declared cardinalities — one Java program rather than three dialects of awk, because
# every one of them is a decision about files on disk and §3 puts the instrument in Java.
#
# It runs HERE, after generation, so the join reads the markers the generator just wrote: a
# section whose snippet was deleted must stop being COVERED in the same run that stopped
# emitting it, not in the next one.
$B/javelinac --libdir $LIBDIR -O0 conformance/gates -o $B/conf-gates.wasm
$B/javelina --jre $B/conf-jre-O0.wasm --root . $B/conf-gates.wasm conformance

# ── the negative half ───────────────────────────────────────────────────────
# The programs the spec says must NOT compile. Their oracle is javelinac's exit code and
# diagnostic, so they cannot live in the suite above — the program that would demonstrate
# the rule is exactly the one that must not build.
sh conformance/run-reject.sh

echo "java e2e conformance: 1 passed, 0 failed"
