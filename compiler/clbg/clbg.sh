#!/bin/sh
# clbg.sh — the Computer Language Benchmarks Game as an outside yardstick.
#
#   sh clbg/clbg.sh            verify: every program at its VERIFICATION N,
#                              output diffed against the published reference
#                              file, in all four compiler/engine configs
#   sh clbg/clbg.sh --time     measure: every program at the game's own N,
#                              min of 3, printed beside the published columns
#
# WHY BOTH. The verify pass is the correctness gate and is cheap; it is what
# says our port computes the task. The time pass is minutes-to-hours per
# program and is only meaningful on an idle machine, so it is never the
# default and never runs in the suite.
#
# WHAT "MATCHES" MEANS. The game's rule is that the ALGORITHM matches the
# description and the OUTPUT matches the reference file — not that the source
# matches the published program. Ports here say in their header comment exactly
# what differs and why (almost always: a java.text formatter replaced, or a
# Java 1.1 construct rewritten). nbody is compared with a tolerance because its
# own description says to: `ndiff -abserr 1.0e-8`.
#
# THE PUBLISHED COLUMNS are read from clbg/published.csv, extracted from the
# game's own public/data/data.csv. Only SINGLE-THREADED rows are used
# (cpu-time within 15% of elapsed): javelina has no threads at all, so timing
# it against a 4-way-parallel C program measures the thread count, not the
# engine. That selection is the difference between a fair number and a
# flattering one.
set -e
cd "$(dirname "$0")/.."           # compiler/
B=build
REF=clbg/ref
JAVA=clbg/java
CSV=clbg/published.csv
OUT=$B/clbg
mkdir -p $OUT

MODE=verify
[ "$1" = "--time" ] && MODE=time

# name:verify-N:measure-N   (measure-N is the game's own argument)
PROGRAMS="
binarytrees:10:21
fannkuchredux:7:12
mandelbrot:200:16000
mandelbrot_simd:200:16000
spectralnorm:100:5500
spectralnorm_simd:100:5500
nbody:1000:50000000
fasta:1000:25000000
revcomp:1000:25000000
regexredux:1000:5000000
knucleotide:1000:25000000
pidigits:30:10000
"

# A _simd entry is a second ENTRY for the same task, not a second task: it
# checks against the scalar program's reference and is timed beside it. The pair
# is the whole point — the published C entry for both tasks is itself
# hand-written SSE2, so the scalar row is comparable to javavm/javaxint and the
# SIMD row is comparable to the C one.
#
# The two differ in what the pair prices. spectralnorm's inner loop reads a
# double[] every step and there is no vector load from a Java array, so its
# lanes are rebuilt per element: that row measures lane-assembly overhead
# against arithmetic width. mandelbrot's inner loop touches no arrays at all —
# Cr/Ci come from the pixel index — so lane assembly is paid once per pixel pair
# and amortised over up to 50 iterations. The delta between the two pairs is
# what a linear-memory vector load would be worth.
refname() {
    case "$1" in
        spectralnorm_simd) echo spectralnorm ;;
        mandelbrot_simd)   echo mandelbrot ;;
        *)                 echo "$1" ;;
    esac
}

# revcomp, knucleotide and regexredux take no argument: the task feeds them a
# fasta-generated subject on stdin, and the N in the table is the fasta N that
# generates it. So fasta must already be built when they run, which the compile
# loop guarantees by running over the whole list first.
reads_stdin() {
    case "$1" in
        revcomp|knucleotide|regexredux) return 0 ;;
        *) return 1 ;;
    esac
}

# The subject for a given fasta N, generated once and reused across configs.
subject_for() {
    _n=$1
    if [ ! -f "$OUT/subject-$_n.txt" ]; then
        $B/javelina --jre $OUT/jre-O.wasm -nojit $OUT/fasta-O.wasm "$_n" \
            > "$OUT/subject-$_n.txt" 2>/dev/null
    fi
    echo "$OUT/subject-$_n.txt"
}

fail=0

echo "== compiling (both optimization levels) =="
$B/javelinac --mode jre --libdir lib/java -O0 -o $OUT/jre-O0.wasm > /dev/null
$B/javelinac --mode jre --libdir lib/java -O  -o $OUT/jre-O.wasm  > /dev/null
for entry in $PROGRAMS; do
    name=${entry%%:*}
    for lvl in O0 O; do
        if ! $B/javelinac -$lvl --libdir lib/java $JAVA/$name.java \
                -o $OUT/$name-$lvl.wasm > $OUT/$name-$lvl.compile 2>&1; then
            sed 's/^/    /' $OUT/$name-$lvl.compile
            echo "clbg: $name failed to compile at -$lvl"
            exit 1
        fi
    done
done

if [ "$MODE" = verify ]; then
    echo "== verifying against the published reference output =="
    for entry in $PROGRAMS; do
        name=${entry%%:*}
        rest=${entry#*:}
        n=${rest%%:*}
        ref=$REF/$(refname $name)-output.txt
        agree=yes
        for cfg in O0-nojit O0-jit O-nojit O-jit; do
            lvl=${cfg%%-*}
            tier=-${cfg#*-}
            if reads_stdin "$name"; then
                $B/javelina --jre $OUT/jre-$lvl.wasm $tier $OUT/$name-$lvl.wasm \
                    < "$(subject_for $n)" > $OUT/$name-$cfg.out 2>/dev/null
            else
                $B/javelina --jre $OUT/jre-$lvl.wasm $tier $OUT/$name-$lvl.wasm $n \
                    > $OUT/$name-$cfg.out 2>/dev/null
            fi
            if ! cmp -s "$ref" $OUT/$name-$cfg.out; then
                agree=no
                echo "  FAIL  $name ($cfg, N=$n) differs from $ref"
                cmp "$ref" $OUT/$name-$cfg.out | head -2 | sed 's/^/          /'
                fail=1
            fi
        done
        [ $agree = yes ] && printf '  ok    %-18s N=%-6s all four configs byte-identical\n' "$name" "$n"
    done
    [ $fail -ne 0 ] && exit 1
    echo "clbg: every program matches its published reference in every config"
    exit 0
fi

# ── Machine calibration ───────────────────────────────────────────────────────
#
# This box is not the game's box, so a raw millisecond count here means nothing
# against a published second count there. The two C entries in clbg/c are built
# with the published command line and timed here; published/local is the scaling
# constant. Two of them, not one, because they stress different hardware — SSE
# integer shuffling and AVX floating point — and a single constant would hide
# which workload it was derived from. The spread between them is the honest
# error bar on every scaled number below.
CDIR=clbg/c
echo "== machine calibration (min of 3) =="
echo "   Each C entry is built TWICE. -march=ivybridge is the published command"
echo "   line, native for the game's i5-3330 and NOT native here, so that build"
echo "   is handicapped on this box and inflates the ratio — which would flatter"
echo "   every scaled number. -march=native is what this box can actually do."
echo "   The NATIVE ratio is the one used, because it is the conservative one."
printf '  %-24s %9s %9s %9s %8s\n' entry ivybridge native published ratio
calib_sum=0
calib_n=0
time_best() {                      # $1 = binary, $2 = N -> best ms of 3
    _b=
    for _r in 1 2 3; do
        _t0=$(date +%s%N)
        "$1" "$2" > /dev/null 2>&1
        _t1=$(date +%s%N)
        _ms=$(( (_t1 - _t0) / 1000000 ))
        [ -z "$_b" ] && _b=$_ms
        [ "$_ms" -lt "$_b" ] && _b=$_ms
    done
    echo "$_b"
}
for c in fannkuchredux.gcc-4:12:14.040 nbody.gcc-9:50000000:2.099; do
    cname=${c%%:*}
    crest=${c#*:}
    cn=${crest%%:*}
    cpub=${crest#*:}
    [ -x "$CDIR/$cname" ] || { printf '  %-24s (not built)\n' "$cname"; continue; }
    civy=$(time_best "$CDIR/$cname" "$cn")
    cnat=$civy
    [ -x "$CDIR/$cname.native" ] && cnat=$(time_best "$CDIR/$cname.native" "$cn")
    ratio=$(awk -v l="$cnat" -v p="$cpub" 'BEGIN { printf "%.2f", (l/1000.0)/p }')
    printf '  %-24s %7sms %7sms %8ss %7sx\n' "$cname" "$civy" "$cnat" "$cpub" "$ratio"
    calib_sum=$(awk -v s="$calib_sum" -v r="$ratio" 'BEGIN { print s + r }')
    calib_n=$((calib_n + 1))
done
CAL=$(awk -v s="$calib_sum" -v n="$calib_n" 'BEGIN { print (n>0 ? s/n : 1) }')
printf '  this box runs the C references %sx the game i5-3330 wall time (native build)\n' \
       "$(awk -v c="$CAL" 'BEGIN { printf "%.2f", c }')"

# Best published C time for a task, and the two Java rows, from published.csv.
pub() { awk -F, -v b="$1" -v i="$2" \
    '$1==b && $2==i { if (m=="" || $5+0 < m+0) m=$5 } END { print (m=="" ? "-" : m) }' "$CSV"; }
bestc() { awk -F, -v b="$1" \
    '$1==b && ($2=="gcc"||$2=="clang") { if (m=="" || $5+0 < m+0) m=$5 } END { print (m=="" ? "-" : m) }' "$CSV"; }

echo ""
echo "== measuring at the game's own N (min of 3) =="
echo "   ours in ms on THIS box; scaled = ours / calibration, i.e. estimated on the game's box"
printf '  %-18s %10s %10s %10s %10s %10s %9s %9s %9s\n' \
       program O0-nojit O0-jit O-nojit O-jit 'O-jit~i5' bestC javavm javaxint
for entry in $PROGRAMS; do
    name=${entry%%:*}
    n=${entry##*:}
    printf '  %-18s' "$name"
    ojit=
    for cfg in O0-nojit O0-jit O-nojit O-jit; do
        lvl=${cfg%%-*}
        tier=-${cfg#*-}
        best=
        for rep in 1 2 3; do
            t0=$(date +%s%N)
            if reads_stdin "$name"; then
                $B/javelina --jre $OUT/jre-$lvl.wasm $tier $OUT/$name-$lvl.wasm \
                    < "$(subject_for $n)" > /dev/null 2>&1
            else
                $B/javelina --jre $OUT/jre-$lvl.wasm $tier $OUT/$name-$lvl.wasm $n > /dev/null 2>&1
            fi
            t1=$(date +%s%N)
            ms=$(( (t1 - t0) / 1000000 ))
            [ -z "$best" ] && best=$ms
            [ "$ms" -lt "$best" ] && best=$ms
        done
        printf ' %9sms' "$best"
        [ "$cfg" = O-jit ] && ojit=$best
    done
    task=$(refname "$name")
    printf ' %9ss %9s %9s %9s\n' \
        "$(awk -v o="$ojit" -v c="$CAL" 'BEGIN { printf "%.2f", (o/1000.0)/c }')" \
        "$(bestc "$task")" "$(pub "$task" javavm)" "$(pub "$task" javaxint)"
done
echo ""
echo "published single-threaded reference times: clbg/published.csv (cpu seconds)"
echo "O-jit~i5 is our -O/-jit wall time divided by the C calibration ratio, so it is"
echo "comparable with the bestC/javavm/javaxint columns; the calibration is two"
echo "programs, and their spread is the error bar."
