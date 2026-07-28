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
# each output match its COMPOSED expectation — is conformance/judge, a Java program running on
# javelina, per crisp-tallying-chapters §3: "the compiler's main job is to stress the VM, so
# the instrument stays a Java program compiled by javelinac. A C tool, a native binding or a
# new BBQ backend all delete the instrument and keep the scaffolding." Shell that greps and
# diffs is the same mistake in a cheaper costume.
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
.SECONDARY: $(WASMS)

# THE TOOLS AND THE JRE ARE PREREQUISITES, not ambient facts. Without them make answers the
# wrong question: it asks "is this .wasm newer than its .java" when what makes a result valid is
# "was it produced by THIS compiler and checked against THIS jre".
#
# Both halves bit. A javelinac fix leaves every unchanged case's .wasm in place, so the corpus
# reports green on modules built by the previous compiler -- masked only accidentally, because
# the generator rewrites every .java each run and drags the mtimes forward. And run.sh rebuilds
# conf-jre-O0.wasm on every run, so a case could be executed against a jre being rewritten
# underneath it; that is a truncated module, which javelina rejects with a nonzero exit and no
# output at all. It is what "Case38 died at run time (-nojit)" with an empty log was: two
# overlapping test runs, one replacing the jre while the other read it.
#
# Declaring the dependency does not by itself make concurrent runs safe -- nothing here can --
# but it removes the silent-stale half, and it makes the ordering explicit rather than lucky.
$(OUT)/%.wasm: $(OUT)/%.java $(JAVELINAC)
	@$(JAVELINAC) --libdir $(LIBDIR) -O0 $< -o $@ 2>$(@:.wasm=.compile.log) \
	  || { echo "  FAIL  $* did not compile"; sed 's/^/        | /' $(@:.wasm=.compile.log); exit 1; }

# javelina exits nonzero on a trap, so a case that dies at run time fails its own rule and
# make reports THAT case — rather than a flag checked after every other case has run.
$(OUT)/%.nojit.out: $(OUT)/%.wasm $(JAVELINA) $(JRE)
	@$(JAVELINA) --jre $(JRE) -nojit $< > $@ 2>&1 \
	  || { echo "  FAIL  $* died at run time (-nojit)"; sed 's/^/        | /' $@; exit 1; }

$(OUT)/%.jit.out: $(OUT)/%.wasm $(JAVELINA) $(JRE)
	@$(JAVELINA) --jre $(JRE) -jit $< > $@ 2>&1 \
	  || { echo "  FAIL  $* died at run time (-jit)"; sed 's/^/        | /' $@; exit 1; }
