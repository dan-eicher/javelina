# javelina — the WASM engine and the Java 1.0 compiler that targets it.
#
# The two projects build independently; this is the top of both. The engine is
# built first everywhere it matters, because the compiler's execution tests link
# its prebuilt objects and run the modules they assemble inside it.

.PHONY: all test test-vm test-compiler test-conformance test-cli test-bench \
        test-one baseline clean help

help:
	@echo "javelina"
	@echo ""
	@echo "  make test              both suites: VM, then compiler, then the CLI gate"
	@echo "  make test-vm           the engine's gates (interp == JIT, GC, c-api, conformance)"
	@echo "  make test-compiler     the compiler's suites"
	@echo "  make test-conformance  the official WebAssembly testsuite, executed"
	@echo "  make test-java-conformance  the Java e2e corpus (E7.4 — not built yet)"
	@echo "  make test-cli          the shipped javelinac/javelina binaries, end to end"
	@echo "  make test-bench        the benchmark checksum gate"
	@echo ""
	@echo "  make test-one T=<name> one suite, either project, built incrementally"
	@echo "      e.g. T=test_gc_los, T=test_sema"
	@echo ""
	@echo "  make baseline          re-measure and print what docs/test-baseline.md pins"

all:
	$(MAKE) -C wasm
	$(MAKE) -C compiler

# The full gate, in dependency order. The VM is the compiler's execution
# oracle, so a broken engine should report as a broken engine rather than as
# twenty confusing compiler failures.
test:
	@echo "── VM ──────────────────────────────────────────────────────"
	@$(MAKE) --no-print-directory -C wasm test
	@echo ""
	@echo "── compiler ────────────────────────────────────────────────"
	@$(MAKE) --no-print-directory -C compiler test
	@echo ""
	@echo "── shipped binaries (CLI) ──────────────────────────────────"
	@$(MAKE) --no-print-directory -C compiler test-cli
	@echo ""
	@echo "── benchmarks (checksum gate) ──────────────────────────────"
	@$(MAKE) --no-print-directory -C compiler test-bench
	@echo ""
	@echo "── Java e2e conformance ────────────────────────────────────"
	@$(MAKE) --no-print-directory test-java-conformance

# The e2e layer: real .java programs with expected stdout and exit code, driven
# through the SHIPPED javelinac + javelina binaries, both tiers. E7.4 builds it;
# the slot is reserved here so it is part of the default gate the day it lands.
#
# Until then this REPORTS its absence and does not fail. It must never pass
# silently: a missing gate that prints nothing is indistinguishable from a
# passing one, which is the same defect as an exclusion counter stuck at zero.
# test/test_cli.sh (in the gate above) is the seed — it already drives both
# binaries end to end for argv, exit codes and -jit/-nojit agreement.
.PHONY: test-java-conformance
test-java-conformance:
	@if [ -d conformance ]; then \
	    sh conformance/run.sh; \
	else \
	    echo "  SKIP  conformance/ not built yet (E7.4) — e2e breadth is NOT covered."; \
	    echo "        Seeded by compiler/test/test_cli.sh, which runs in the gate above."; \
	fi

test-vm:
	@$(MAKE) --no-print-directory -C wasm test

test-compiler:
	@$(MAKE) --no-print-directory -C compiler test

# The oracle on its own: the pinned testsuite submodule, executed both tiers.
test-conformance:
	@$(MAKE) --no-print-directory -C wasm test-one T=test_wast || true
	@cd wasm/test && ../build/test_wast ../../testsuite/*.wast regress_*.wast

test-cli:
	@$(MAKE) --no-print-directory -C compiler test-cli

test-bench:
	@$(MAKE) --no-print-directory -C compiler test-bench

# Route a single suite to whichever project owns it.
test-one:
	@if [ -f wasm/test/$(T).c ]; then $(MAKE) --no-print-directory -C wasm test-one T=$(T); \
	elif [ -f compiler/test/$(T).c ]; then $(MAKE) --no-print-directory -C compiler test-one T=$(T); \
	else echo "no such suite: $(T)"; exit 2; fi

# Re-measure what docs/test-baseline.md pins, so a claim about the numbers is
# always something that was just run rather than something remembered.
baseline:
	@echo "Re-running both suites with timing. Compare against docs/test-baseline.md."
	@/usr/bin/time -f "VM:       WALL %e s  PEAK %M KB" $(MAKE) --no-print-directory -C wasm test
	@/usr/bin/time -f "compiler: WALL %e s  PEAK %M KB" $(MAKE) --no-print-directory -C compiler test

clean:
	$(MAKE) -C wasm clean
	$(MAKE) -C compiler clean
