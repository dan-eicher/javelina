# test.mk — the VM's test build, as real make targets.
#
# Included at the END of Makefile so every variable it reads is defined.
#
# What this replaces: one shell recipe that recompiled the shared sources once
# per gate — the nine c-lite sources rebuilt for each of a dozen c-lite tests,
# the wat parser for each of five, wasm_capi for each of four — and sent every
# suite's output to /dev/null, so a failure printed one line and discarded the
# evidence.
#
# Every gate that existed before exists here, with the same link set, the same
# working directory and the same arguments. The groups below are the ones the
# Makefile's comments already described; making them explicit is the point.

LOGS := $(B)/logs

# The -MMD dependency files this Makefile includes list HEADERS as
# prerequisites, so $^ is not a link line. Link only the sources and objects.
LINK = $(filter %.c %.o,$^)

# ── shared objects (previously recompiled per gate) ──────────────────────────
$(B)/jav_view_nav.o:        src/jav_view_nav.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_module_index.o:    src/jav_module_index.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_module_validate.o: src/jav_module_validate.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_instance.o:        src/jav_instance.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_extern.o:          src/jav_extern.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_error.o:           src/jav_error.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_view_reader.o:     $(GEN)/jav_view_reader.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/bbq_lite.o:            $(RT)/bbq_lite.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_load.o:            src/jav_load.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_module_struct.o:   src/jav_module_struct.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/jav_validate_module.o: src/jav_validate_module.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/wasm_capi.o:           src/wasm_capi.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -c $< -o $@
$(B)/wat_driver.o:          src/wat_driver.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) -c $< -o $@
$(B)/wat_parser.o:          $(GEN)/wat_parser.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) -c $< -o $@
$(B)/bbq_htree_capi.o:      $(CRT)/bbq_htree.c | $(B)
	$(CC) $(CFLAGS) -c $< -o $@

# The c-lite load path as objects. jav_utf8.o is shared with the owning group;
# ENGINE_OBJS deliberately excludes it, so nothing is linked twice.
CLITE_OBJS := $(B)/jav_view_nav.o $(B)/jav_module_index.o $(B)/jav_module_validate.o \
              $(B)/jav_module_struct.o $(B)/jav_instance.o $(B)/jav_extern.o $(B)/jav_error.o \
              $(B)/jav_view_reader.o $(B)/bbq_lite.o $(B)/jav_utf8.o
WAT_OBJS   := $(B)/wat_driver.o $(B)/wat_parser.o
CAPI_OBJS  := $(B)/wasm_capi.o $(WAT_OBJS) $(B)/jav_reader.o $(B)/jav_writer.o \
              $(TOML_OBJS) $(CLITE_OBJS) $(B)/jav_load.o $(ENGINE_OBJS)

# ── gate groups ─────────────────────────────────────────────────────────────
# Standalone: deliberately minimal include sets, so these prove the unit builds
# without the generated runtime. Run from the tree root (no fixtures).
$(B)/test_subtype: test/test_subtype.c src/jav_subtype.c | $(B)
	$(CC) -O2 -std=c11 -Wall -Werror -Isrc $(LINK) -o $@
$(B)/test_storage: test/test_storage.c | $(B)
	$(CC) -O2 -std=c11 -Wall -Werror -Isrc -Isrc/gen -Isrc/immix -I$(RT) -I$(CRT) $< -o $@
# The tier-2 signature vocabulary, checked against its two sibling artifacts.
# Reads them as text from src/gen, so it runs from the tree root like the rest
# of this group.
$(B)/test_sigtab: test/test_sigtab.c $(GEN)/jav_sigtab.h | $(B)
	$(CC) -O2 -std=c11 -Wall -Werror -Isrc/gen $< -o $@
# The tree builder, linked with the crt arena and nothing else — no engine, no
# validator: the trees are the only variable.
$(B)/test_ttree: test/test_ttree.c $(B)/jav_ttree.o $(B)/bbq_arena.o | $(B)
	$(CC) $(CFLAGS) $(LINK) -o $@
$(B)/test_leb: test/test_leb.c | $(B)
	$(CC) -O2 -std=c11 -Wall -Werror -I$(RT) -I$(CRT) $< -o $@
# jav_module_wf's §5.5 rules, one hand-built violation each. Links only the
# validator and the reader's free helpers — no runtime, no parsing — so the
# violation under test is the only variable.
$(B)/test_module_wf: test/test_module_wf.c src/jav_validate_module.c | $(B)
	$(CC) $(CFLAGS) -I../testkit $(LINK) -lm -o $@

ROOT_TESTS := test_subtype test_storage test_leb test_module_wf test_sigtab test_ttree

# Immix: the GC compiled standalone — pure C + bbq_vec, no generated runtime.
$(IMMIX_TESTS:%=$(B)/%): $(B)/%: test/%.c $(IMMIX_SRC) | $(B)
	$(CC) $(IMMIX_CFLAGS) $(LINK) -o $@

# The validated interp == JIT gates.
$(TESTS:%=$(B)/%): $(B)/%: test/%.c $(OBJS) | $(B)
	$(CC) $(CFLAGS) $(LINK) -lm -o $@

# The stencil table's structural gate reads the generated HEADER and nothing else — no
# engine, no jit meta. That is the point of it: test_jit_holes needs the meta tables to
# decide whether a hole is patchable, so broken meta reds it for a reason that is not the
# table, while this one still answers "is the committed table self-consistent". Linking the
# engine into it would couple the two again.
$(B)/test_jit_table: test/test_jit_table.c $(GEN)/jav_stencil_table.h | $(B)
	$(CC) $(CFLAGS) test/test_jit_table.c -o $@

# The c-lite load path: CLITE + engine, never the owning reader.
CLITE_TESTS := test_skeleton test_div test_instantiate test_module_index \
               test_gc_funcref test_addrtype_spec test_gc_roots_real \
               test_gc_refforms test_module_validate test_gc_validation test_load
$(CLITE_TESTS:%=$(B)/%): $(B)/%: test/%.c $(CLITE_OBJS) $(ENGINE_OBJS) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function $(LINK) -lm -o $@

# The §5.5 structure gate, driven through the loader entry the engine uses.
$(B)/test_module_struct: test/test_module_struct.c $(B)/jav_load.o $(CLITE_OBJS) $(ENGINE_OBJS) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -I../testkit $(LINK) -lm -o $@

# test_align is the c-lite group plus the toml table.
$(B)/test_align: test/test_align.c $(CLITE_OBJS) $(ENGINE_OBJS) $(TOML_OBJS) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude $(LINK) -lm -o $@

# The owning reader/writer gates.
OWNING_TESTS := test_roundtrip test_func test_types test_sections test_malformed
$(OWNING_TESTS:%=$(B)/%): $(B)/%: test/%.c $(OBJS) | $(B)
	$(CC) $(CFLAGS) $(LINK) -lm -o $@

$(B)/test_instr: test/test_instr.c $(OBJS) $(TOML_OBJS) | $(B)
	$(CC) $(CFLAGS) -Iinclude -I$(PEGRT) $(LINK) -lm -o $@
$(B)/test_wat: test/test_wat.c $(WAT_OBJS) $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o $(TOML_OBJS) $(B)/bbq_arena.o | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) $(LINK) -lm -o $@
$(B)/test_water: test/test_water.c $(WAT_OBJS) $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o $(TOML_OBJS) $(B)/bbq_arena.o | $(B)
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -Wno-unused-function -Iinclude -I$(PEGRT) $(LINK) -lm -o $@

# water's disassembler half: §7.6 over the decoded instruction tree, and the §6
# emitter over it. The link set is ENUMERATED rather than borrowed from $(OBJS),
# because what is ABSENT from it is the contract: no validate.o, no
# jav_module_index.o, no jav_view_reader.o, no jav_ttree.o. water and the engine are
# separate tools; what they share is generated output — jav_reader.o (bbqc, from
# wasm.bbq) and the opgen tables that are headers — plus jav_subtype.o, the one §3.3
# lattice. A second reading of the subtype relation is the one duplication with no
# upside, so it is linked and not re-derived.
WATOUT_OBJS  := $(B)/wat_check.o $(B)/wat_emit.o $(B)/wat_tree.o $(B)/wat_layout.o \
                $(B)/jav_reader.o $(B)/jnm_reader.o $(B)/jav_subtype.o \
                $(B)/jav_error.o $(B)/jav_utf8.o $(B)/bbq_arena.o $(B)/bbq_hmap.o \
                $(B)/bbq_htree.o
$(B)/jnm_reader.o: $(GEN)/jnm_reader.c $(GEN)/jnm_types.h | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/wat_check.o: src/wat_check.c | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/wat_emit.o:  src/wat_emit.c $(GEN)/wat_tnode.h $(GEN)/wat_render.h $(GEN)/wat_layout.h | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
$(B)/wat_tree.o:  src/wat_tree.c src/wat_tree.h $(GEN)/wat_tnode.h $(GEN)/wat_render.h | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@
# The generated labeller/reducer over the render tree (burgc's C backend).
$(B)/wat_layout.o: $(GEN)/wat_layout.c $(GEN)/wat_layout.h src/wat_layout_burg.h $(GEN)/wat_tnode.h | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -c $< -o $@

WATOUT_TESTS := test_wat_check test_wat_fold test_wat_layout
$(WATOUT_TESTS:%=$(B)/%): $(B)/%: test/%.c $(WATOUT_OBJS) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function $(LINK) -lm -o $@
# The layout gates compare the generated artifacts against each other and the
# peg inventory, so they rebuild when any of those move.
$(B)/test_wat_layout: $(GEN)/wat_tnode.h $(GEN)/wat_render.h $(GEN)/wat_layout.burg spec/wat.peg
# test_wat_emit and test_wat_custom additionally REREAD their own renderings
# through the .wat assembler (token separation, float exactness and the §7.7
# byte-identity gates are real round trips), so they link both directions —
# as the shipped water does.
$(B)/test_wat_emit $(B)/test_wat_custom: $(B)/%: test/%.c $(WATOUT_OBJS) $(WAT_OBJS) $(B)/jav_writer.o | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) $(LINK) -lm -o $@
# The CLI's contracts, held from outside: test_wat_cli drives the water BINARY
# through system(), so the binary is a build dependency.
$(B)/test_wat_cli: test/test_wat_cli.c $(B)/water $(WAT_OBJS) $(B)/jav_reader.o \
                   $(B)/jav_writer.o $(B)/jav_utf8.o $(TOML_OBJS) $(B)/bbq_arena.o | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) \
	  $(filter %.c %.o,$^) -lm -o $@

# The conformance runner: the official testsuite, executed.
$(B)/test_wast: test/test_wast.c test/wast_exec.c $(B)/wasm_capi.o $(WAT_OBJS) \
                $(B)/jav_validate_module.o $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o \
                $(TOML_OBJS) $(B)/bbq_arena.o $(B)/jav_load.o $(B)/jav_view_nav.o \
                $(B)/jav_module_index.o $(B)/jav_module_validate.o $(B)/jav_module_struct.o \
                $(B)/jav_instance.o \
                $(B)/jav_extern.o $(B)/jav_error.o $(B)/bbq_htree_capi.o $(B)/jav_view_reader.o \
                $(B)/bbq_lite.o $(B)/validate.o $(B)/jav_subtype.o $(B)/interp.o \
                $(B)/jav_runtime.o $(B)/gen_interp.o $(B)/jit_driver.o $(B)/jav_ttree.o \
                $(B)/jav_tile.o $(B)/jav_eqsat.o $(B)/egraph.o \
                $(B)/bbq_hmap.o $(B)/wat_check.o $(B)/wat_emit.o $(B)/wat_tree.o \
                $(B)/wat_layout.o $(B)/jnm_reader.o $(GC_OBJS) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) $(LINK) -lm -o $@

# The public wasm.h surface.
CAPI_TESTS := test_capi test_capi_gc test_capi_jit
$(CAPI_TESTS:%=$(B)/%): $(B)/%: test/%.c $(CAPI_OBJS) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) $(LINK) -lm -o $@
# ── the embeddable library artifact ─────────────────────────────────────────
# libjavelina.a is exactly what a third-party embedder links: the wasm-c-api
# shim, the zero-copy (c-lite) load path, and the engine. It deliberately omits
# the .wat assembler and the owning reader/writer — those are tooling and tests,
# not the embedding surface. Paired with the public include/wasm.h, this archive
# is the whole contract.
LIB_OBJS := $(B)/wasm_capi.o $(CLITE_OBJS) $(B)/jav_load.o $(ENGINE_OBJS)
$(B)/libjavelina.a: $(LIB_OBJS) | $(B)
	ar rcs $@ $(LIB_OBJS)
.PHONY: lib
lib: $(B)/libjavelina.a

# `make PIC=1 lib-pic-check` — the archive really is position-independent.
#
# A host that is itself a shared library cannot link a non-PIC archive member; ld refuses
# it with "relocation R_X86_64_32S ... recompile with -fPIC", and that refusal is what put
# the PIC=1 variant in the Makefile (Inkscape links its core as libinkscape_base.so). The
# check reads the ARCHIVE rather than trusting the flag, because -fPIC rides on CFLAGS and
# an object built by some other recipe — STENCIL_CFLAGS, a hand-written rule — would not
# carry it and would poison the archive for exactly one consumer: the one that is not us.
# Run it against build/ and it SHOULD fail; that is the control, not a bug.
.PHONY: lib-pic-check
lib-pic-check: $(B)/libjavelina.a
	@command -v readelf >/dev/null || { echo "  readelf not found — cannot verify $(B)/libjavelina.a is PIC"; exit 1; }
	@n=$$(readelf -r $(B)/libjavelina.a | awk '/^Relocation section/{s=$$3} s !~ /debug/ && /R_X86_64_32S?[[:space:]]/{n++} END{print n+0}'); \
	 echo "  $(B)/libjavelina.a: $$n absolute relocations in allocatable sections (a shared-library host requires 0)"; \
	 [ "$$n" -eq 0 ] || { echo "  → rebuild with PIC=1"; exit 1; }

# The conformance story on its own: the pinned official testsuite through the
# interpreter and every JIT tier, with the runner's full output on the terminal
# — the numbers the README quotes. The `test` gate below runs the same legs,
# logged and summarized; this target exists so `make conformance` at the root
# reproduces the README's table without running everything else.
.PHONY: conformance
conformance: $(B)/test_wast
	@( cd test && ../$(B)/test_wast ../../testsuite/*.wast regress_*.wast ) || exit 1
	@for jt in $(JIT_TIERS); do \
	  echo ""; echo "── tier-$$jt ──"; \
	  ( cd test && ../$(B)/test_wast --tier=$$jt ../../testsuite/*.wast regress_*.wast ) || exit 1; \
	done

# embed.c links THE ARCHIVE + wasm.h and nothing else — it demonstrates the
# actual artifact an embedder consumes, and the test gate builds it this way.
$(B)/embed: examples/embed.c $(B)/libjavelina.a | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude examples/embed.c $(B)/libjavelina.a -lm -o $@

# ── the naive-producer corpus (PIN E3) ──────────────────────────────────────
# Kernels lowered the way template producers lower, each with a hand-clean
# twin; water assembles them at build time, and the consumers are EMBEDDERS —
# the archive + wasm.h + the jav_ extension seam, never engine objects or the
# in-process assembler. test_bench_naive is the suite leg (checksum identity
# across tiers and forms + the rewrite pins); run_naive is the timed matrix
# and runs only on the bench handshake (`make bench-naive`).
NAIVE_KERNELS := k_addr k_poly k_wide k_smand k_vmand
NAIVE_WASM := $(foreach k,$(NAIVE_KERNELS),$(B)/naive/$(k)_naive.wasm $(B)/naive/$(k)_clean.wasm)
$(B)/naive/%.wasm: bench/naive/%.wat $(B)/water
	@mkdir -p $(B)/naive
	$(B)/water $< -o $@
$(B)/test_bench_naive: test/test_bench_naive.c $(B)/libjavelina.a $(NAIVE_WASM) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude test/test_bench_naive.c $(B)/libjavelina.a -lm -o $@
$(B)/run_naive: bench/run_naive.c $(B)/libjavelina.a $(NAIVE_WASM) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude bench/run_naive.c $(B)/libjavelina.a -lm -o $@
.PHONY: bench-naive
bench-naive: $(B)/run_naive $(NAIVE_WASM)
	./$(B)/run_naive $(B)/naive

# Everything that is a plain "build it, run it in test/, expect exit 0" gate.
PLAIN_TESTS := $(IMMIX_TESTS) $(TESTS) $(CLITE_TESTS) test_align test_module_struct $(OWNING_TESTS) \
               test_instr test_wat test_water $(WATOUT_TESTS) test_wat_emit test_wat_custom \
               test_wat_cli $(CAPI_TESTS) test_bench_naive test_jit_table
# ...minus the few that take an argument, handled by name below.
ARGV_add_wasm := test_skeleton test_load test_roundtrip test_func

ALL_TESTS := $(ROOT_TESTS) $(PLAIN_TESTS)

.SECONDARY:

# ── running ─────────────────────────────────────────────────────────────────
# `make test-one T=test_gc_los` — build and run exactly one gate.
.PHONY: test-one
test-one: $(B)/$(T)
	@mkdir -p $(LOGS)
	@if ( cd test && ../$(B)/$(T) $(if $(filter $(T),$(ARGV_add_wasm)),add.wasm) ) > $(LOGS)/$(T).log 2>&1; then \
	    echo "  PASS  $(T)"; \
	else \
	    echo "  FAIL  $(T)  (log below, also at $(LOGS)/$(T).log)"; \
	    sed 's/^/      | /' $(LOGS)/$(T).log; exit 1; \
	fi

# The gate: every suite, then the legs that are not C programs — conformance at
# each tier, the tier-3 size pin, the tree sweep, water and embed driven as a
# user drives them, the public headers compiled standalone, and spec/ swept for
# work markers.
#
# That last one is a documentation gate, and it is here because spec/ is not
# documentation: wasm.def, wat.peg and the rest are the sources every generated
# form is derived from, so a comment in them is read as the contract. Two sat in
# wat.peg saying `deduplication TODO` and `(rec …), sub TODO` long after
# wat_typeuse deduped and RecField parsed rec groups — each one an invitation to
# reimplement something that was already there. A marker in spec/ is either work
# that has not happened, in which case it belongs in a plan where it can be
# scheduled, or work that has, in which case it is false. Neither belongs here.
.PHONY: test
test: $(ALL_TESTS:%=$(B)/%) $(B)/test_wast $(B)/embed water
	@mkdir -p $(LOGS); pass=0; fail=0; failed=""; \
	for t in $(ROOT_TESTS); do \
	  if ./$(B)/$$t > $(LOGS)/$$t.log 2>&1; then echo "  PASS  $$t"; pass=$$((pass+1)); \
	  else echo "  FAIL  $$t"; fail=$$((fail+1)); failed="$$failed $$t"; fi; \
	done; \
	for t in $(PLAIN_TESTS); do \
	  case " $(ARGV_add_wasm) " in *" $$t "*) a=add.wasm;; *) a=;; esac; \
	  if ( cd test && ../$(B)/$$t $$a ) > $(LOGS)/$$t.log 2>&1; then echo "  PASS  $$t"; pass=$$((pass+1)); \
	  else echo "  FAIL  $$t"; fail=$$((fail+1)); failed="$$failed $$t"; fi; \
	done; \
	if ( cd test && ../$(B)/test_wast ../../testsuite/*.wast regress_*.wast ) > $(LOGS)/conformance.log 2>&1; then \
	  grep -E "conformance:|gate|reason" $(LOGS)/conformance.log; \
	  echo "  PASS  conformance"; pass=$$((pass+1)); \
	else echo "  FAIL  conformance"; fail=$$((fail+1)); failed="$$failed conformance"; fi; \
	for jt in $(JIT_TIERS); do \
	  if ( cd test && ../$(B)/test_wast --tier=$$jt ../../testsuite/*.wast regress_*.wast ) > $(LOGS)/conformance_t$$jt.log 2>&1; then \
	    grep -E "conformance \[|trap-reason|tier-3 eqsat" $(LOGS)/conformance_t$$jt.log; \
	    echo "  PASS  conformance (tier-$$jt)"; pass=$$((pass+1)); \
	  else echo "  FAIL  conformance (tier-$$jt)"; fail=$$((fail+1)); failed="$$failed conformance_t$$jt"; fi; \
	done; \
	if [ -f $(LOGS)/conformance_t2.log ] && [ -f $(LOGS)/conformance_t3.log ]; then \
	  b2=$$(grep "wast tier-2 code:" $(LOGS)/conformance_t2.log | sed 's/.*code: \([0-9]*\) bytes.*/\1/'); \
	  b3=$$(grep "wast tier-2 code:" $(LOGS)/conformance_t3.log | sed 's/.*code: \([0-9]*\) bytes.*/\1/'); \
	  if [ -n "$$b2" ] && [ -n "$$b3" ] && [ "$$b3" -le "$$b2" ]; then \
	    echo "  PASS  tier-3 code <= tier-2 (PIN B-1: $$b3 <= $$b2 bytes; saved $$((b2-b3)))"; pass=$$((pass+1)); \
	  else \
	    echo "  FAIL  tier-3 code <= tier-2 ($$b3 vs $$b2 bytes) — the rewrite made code BIGGER"; \
	    fail=$$((fail+1)); failed="$$failed t3_code_size"; fi; \
	fi; \
	if ( cd test && ../$(B)/test_wast --sweep ../../testsuite/*.wast regress_*.wast ) > $(LOGS)/ttree_sweep.log 2>&1; then \
	  grep -E "tree sweep|tier-2 tiling|declines by|uncovered at" $(LOGS)/ttree_sweep.log; \
	  echo "  PASS  tier-2 tree sweep"; pass=$$((pass+1)); \
	else echo "  FAIL  tier-2 tree sweep"; fail=$$((fail+1)); failed="$$failed ttree_sweep"; fi; \
	if ( cd test && printf '(module (func (export "id") (param i32) (result i32) local.get 0))' \
	       | ../$(B)/water - > ../$(B)/.water_smoke.wasm 2>/dev/null \
	       && ../$(B)/test_roundtrip ../$(B)/.water_smoke.wasm ) > $(LOGS)/water.log 2>&1; then \
	  echo "  PASS  water (cli → round-trippable .wasm)"; pass=$$((pass+1)); \
	else echo "  FAIL  water (cli → round-trippable .wasm)"; fail=$$((fail+1)); failed="$$failed water"; fi; \
	if ( cd test && printf '(module (func (export "add") (param i32 i32) (result i32) local.get 0 local.get 1 i32.add))' \
	       | ../$(B)/water - > ../$(B)/.embed.wasm 2>/dev/null \
	       && ../$(B)/embed ../$(B)/.embed.wasm ) > $(LOGS)/embed.log 2>&1; then \
	  echo "  PASS  embed (public wasm.h → instantiate + call)"; pass=$$((pass+1)); \
	else echo "  FAIL  embed (public wasm.h → instantiate + call)"; fail=$$((fail+1)); failed="$$failed embed"; fi; \
	for h in include/*.h; do \
	  if $(CC) -std=c11 -Wall -Wextra -Werror -Iinclude -c -x c -include "$$h" /dev/null -o /dev/null > $(LOGS)/hdrsweep.log 2>&1; then \
	    echo "  PASS  public header compiles standalone ($$h)"; pass=$$((pass+1)); \
	  else echo "  FAIL  public header not standalone ($$h)"; fail=$$((fail+1)); failed="$$failed hdr"; \
	    sed 's/^/      | /' $(LOGS)/hdrsweep.log; fi; \
	done; \
	if grep -rnIE '\b(TODO|FIXME|XXX|HACK)\b' spec/ > $(LOGS)/specmarkers.log 2>&1; then \
	  echo "  FAIL  spec/ carries a work marker — see below"; \
	  sed 's/^/      | /' $(LOGS)/specmarkers.log; \
	  fail=$$((fail+1)); \
	else echo "  PASS  spec/ carries no work markers"; pass=$$((pass+1)); fi; \
	for t in $$failed; do \
	  echo ""; echo "── $$t ─────────────────────────────────────────"; \
	  sed 's/^/  | /' $(LOGS)/$$t.log; \
	done; \
	echo ""; echo "── $$pass passed, $$fail failed ──"; \
	[ $$fail -eq 0 ]
