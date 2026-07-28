#!/bin/sh
# run-reject.sh — the NEGATIVE half of the corpus.
#
# A large fraction of the JLS is not "this evaluates to that" but "a compile-time error
# occurs". A running program can never assert those: the program that would demonstrate the
# rule is exactly the program that must not compile. So they get their own corpus, where the
# artifact under test is javelinac's EXIT CODE and its DIAGNOSTIC.
#
# One file per case in conformance/reject/, each carrying two directives:
#
#     // JLS 5.2          the section whose rule the file violates — the SAME marker the
#                         coverage join reads, so a rejection counts as coverage
#     // EXPECT <text>    a substring the diagnostic must contain
#
# EXPECT is what stops this from being a rubber stamp. Without it any file that fails to
# compile passes, including one that fails for a typo — the case would "cover" §5.2 while
# demonstrating nothing about §5.2. Requiring the message to name the right thing means the
# compiler has to reject it for the STATED reason. It is a substring rather than a full match
# so diagnostics can be reworded without a corpus-wide edit; it must still be specific enough
# that a different error does not satisfy it.
#
# Each case is compiled ALONE (with the RTL on --libdir), so one case cannot mask another and
# the diagnostic belongs to the case that expected it.
#
# A case may also be a DIRECTORY of compilation units, which is what chapter 6 needs: access
# control, protected access and package-qualified names are rules ABOUT the boundary between
# packages, so the smallest program that violates one is two files in two packages. The
# directory is handed to javelinac whole (it already takes a directory), and the directives are
# read from whichever unit inside carries them — normally the one holding the offending line.
#
# Usage:  sh conformance/run-reject.sh          compile every case, report, exit nonzero on any failure
#         sh conformance/run-reject.sh -v       also print each accepted diagnostic

set -e
cd "$(dirname "$0")/.."

B=compiler/build
LIBDIR=compiler/lib/java
# ONE tree, and it is the generator's. Every case here is written by conformance/gen from a
# template whose expects is a compile-time error, which is what crisp-tallying-chapters §3
# names as one of the three kinds of `expects` and what §4 means by "it still carries
# sections[] and expects and still goes through the same emitter".
#
# There used to be a hand-written conformance/reject/ beside it. That was the
# subset-chosen-at-authoring-time §4 forbids: the section claim and the expected diagnostic were
# typed into a comment by whoever wrote the program, so nothing tied them to a template and the
# cardinality gate could not see them at all. They are now DERIVED from sections() and expect().
DIR=conformance/generated
VERBOSE=0
[ "$1" = "-v" ] && VERBOSE=1

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

[ -d "$DIR" ] || { echo "  FAIL  run-reject: $DIR missing — the generator did not run"; exit 1; }

# The generated tree also holds Case*.java, which MUST compile. Only Reject_* is ours: a file
# and a directory form, the latter for a rule that needs more than one compilation unit.
for f in "$DIR"/Reject_*.java "$DIR"/Reject_*/; do
    [ -e "$f" ] || continue
    if [ -d "$f" ]; then
        name=$(basename "$f")
        units=$(find "$f" -name '*.java' | sort)
        [ -n "$units" ] || { echo "  FAIL  reject/$name: directory case holds no .java"; fail=$((fail + 1)); continue; }
    else
        name=$(basename "$f" .java)
        units=$f
    fi

    want=$(sed -n 's|^[[:space:]]*//[[:space:]]*EXPECT[[:space:]]\{1,\}||p' $units)
    sec=$(sed -n 's|^[[:space:]]*//[[:space:]]*JLS[[:space:]]\{1,\}\([0-9][0-9.]*\).*|\1|p' $units | head -1)

    # A case with no EXPECT is not a case. It would pass on any failure at all, which is the
    # exact defect this file exists to prevent, so it is a HARD error rather than a skip.
    if [ -z "$want" ]; then
        echo "  FAIL  reject/$name: no \`// EXPECT <text>\` directive — it would pass on any error"
        fail=$((fail + 1))
        continue
    fi
    if [ -z "$sec" ]; then
        echo "  FAIL  reject/$name: no \`// JLS <section>\` marker — the rejection covers nothing"
        fail=$((fail + 1))
        continue
    fi

    # Compiled alone. stdout and stderr both captured: where a diagnostic goes is not part of
    # the rule being tested, and a case must not turn into a stream-routing test.
    if $B/javelinac --libdir $LIBDIR -O0 "$f" -o "$TMP/$name.wasm" > "$TMP/$name.log" 2>&1; then
        echo "  FAIL  reject/$name (JLS $sec): COMPILED — the spec says this is a compile-time error"
        fail=$((fail + 1))
        continue
    fi

    if grep -qF -- "$want" "$TMP/$name.log"; then
        pass=$((pass + 1))
        [ "$VERBOSE" -eq 1 ] && sed 's/^/        | /' "$TMP/$name.log"
    else
        echo "  FAIL  reject/$name (JLS $sec): rejected, but not for the stated reason"
        echo "        expected the diagnostic to contain: $want"
        sed 's/^/        | /' "$TMP/$name.log" | head -8
        fail=$((fail + 1))
    fi
done

# Zero cases is a green run that checked nothing — the same vacuous pass the RESULT-line count
# guards against on the positive side.
if [ $((pass + fail)) -eq 0 ]; then
    echo "  FAIL  run-reject: no cases found in $DIR"
    exit 1
fi

if [ $fail -ne 0 ]; then
    echo "  FAIL  reject corpus: $pass rejected correctly, $fail wrong"
    exit 1
fi

echo "  ....  reject corpus: $pass programs rejected, each naming its own rule"
