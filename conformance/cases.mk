# cases.mk — the driver for the stitched corpus.
#
# One rule per artifact, and make decides what to run. This was a serial `for` loop in shell,
# which is a hand-rolled make with none of make's properties: the 65 cases are independent, so
# a loop spends four minutes doing what -j does in one core's worth of wall clock; an unchanged
# case recompiled every run because nothing tracked that .wasm is newer than .java; and a
# failure mid-loop set a flag and kept going rather than stopping, so the report came four
# minutes after the fact.
#
# It does NOT decide whether the corpus is correct. Make builds and runs; the judgement — does
# each output match its COMPOSED expectation — belongs to a program running on javelina, per
# crisp-tallying-chapters §3: "the compiler's main job is to stress the VM, so the instrument
# stays a Java program compiled by javelinac. A C tool, a native binding or a new BBQ backend
# all delete the instrument and keep the scaffolding." Shell that greps and diffs is the same
# mistake in a cheaper costume.
#
# The case list is a wildcard, so this makefile is invoked SEPARATELY, after the generator has
# written the cases — the wildcard is expanded when make starts, and at the first invocation
# the directory is empty.
#
# Usage:  make -f conformance/cases.mk -j$(nproc) all

OUT     ?= conformance/generated
B       ?= compiler/build
LIBDIR  ?= compiler/lib/java
JAVELINAC ?= $(B)/javelinac
JAVELINA  ?= $(B)/javelina
JRE       ?= $(B)/conf-jre-O0.wasm

CASES := $(wildcard $(OUT)/Case*.java)
WASMS := $(CASES:.java=.wasm)
# Both tiers: a case that agrees with its expectation under the interpreter and not under the
# JIT names a config that is WRONG, which is why gc-torture runs four ways too.
OUTS  := $(CASES:.java=.nojit.out) $(CASES:.java=.jit.out)

.PHONY: all
all: $(OUTS)

# A .wasm is only ever a prerequisite of a .out, so make classes it as an intermediate and
# deletes it on the way out — sixty-five `rm` lines of noise, and the module gone precisely
# when you want to inspect the one that failed.
#
# It does NOT buy incrementality here, and saying so is the point: run-generated.sh wipes the
# case directory every run, because the generator is what decides which cases exist and a
# surviving stale case would keep claiming sections whose snippet was deleted. So every run is
# a full build and make's value is the parallelism, not the dependency edges. Getting the
# edges to pay would need content-addressed staleness rather than mtime, since a deterministic
# generator rewrites byte-identical files with fresh timestamps.
.SECONDARY: $(WASMS)

# A case that fails to compile is a generator bug or a javelinac bug, never a test bug — the
# generator only emits Java it constructed from snippets that each render legal source.
$(OUT)/%.wasm: $(OUT)/%.java
	@$(JAVELINAC) --libdir $(LIBDIR) -O0 $< -o $@ 2>$(@:.wasm=.compile.log) \
	  || { echo "  FAIL  $* did not compile"; sed 's/^/        | /' $(@:.wasm=.compile.log); exit 1; }

# javelina exits nonzero on a trap, so a case that dies at run time fails its own rule and
# make reports THAT case — rather than a flag checked after every other case has run.
$(OUT)/%.nojit.out: $(OUT)/%.wasm
	@$(JAVELINA) --jre $(JRE) -nojit $< > $@ 2>&1 \
	  || { echo "  FAIL  $* died at run time (-nojit)"; sed 's/^/        | /' $@; exit 1; }

$(OUT)/%.jit.out: $(OUT)/%.wasm
	@$(JAVELINA) --jre $(JRE) -jit $< > $@ 2>&1 \
	  || { echo "  FAIL  $* died at run time (-jit)"; sed 's/^/        | /' $@; exit 1; }
