# javelina WASM runtime — build + differential test suite.
# `make test` reproduces the whole pipeline: opgen generates the interpreter +
# stencils + JIT metadata from spec/wasm.def; jitterator turns the compiled
# stencils into the stencil table; then every gate builds and runs (interp == JIT).

OPGEN      := /home/dan/Source/BBQ/build/opgen/opgen
JITTERATOR := /home/dan/Source/BBQ/build/jitterator/jitterator
BBQC       := /home/dan/Source/BBQ/build/bbqc
PEGC       := /home/dan/Source/BBQ/build/pegc/pegc
PEG_FRAMES := spec/peg_frames   # javelina's OWN copy of the pegc C frames; WAT-specific frame
                                # tweaks (the §6.2.5 annotation skip) live here, NOT the shared template
RT         := /home/dan/Source/BBQ/backends/c/runtime
JITRT      := /home/dan/Source/BBQ/jitterator/runtime
CRT        := /home/dan/Source/BBQ/crt
PEGRT      := /home/dan/Source/BBQ/pegc/runtime
BBQ_TEMPLATES := /home/dan/Source/BBQ/backends/render/templates

# TOML reader (pinched from yoctojc) — used by test_instr to read the
# spec-derived opcode table spec/instructions.toml. Vendored generated parser +
# resolver; compiled with -Wall (not -Werror) like other generated artifacts.
TOML_OBJS   = $(B)/toml_parser.o $(B)/toml_doc.o   # bbq_arena.o is in OBJS (test_instr links both)
TOML_INC   := -Isrc/gen -Iinclude -I$(PEGRT) -I$(CRT)

CC      := clang
# The VM substrate (frame_t/vm_t/heap_t) is named to opgen by the `backend "jav_frame.h"`
# directive in spec/wasm.def, which opgen #includes into the generated runtime_api.h.
# -MMD -MP: emit a .d dependency file per object listing EVERY included header — including
# the BBQ runtime headers under $(RT)/$(CRT) that live outside this tree — so a change to,
# say, bbq_read.h rebuilds the objects that include it. No more manual cleans to pick up
# upstream header edits. The .d files are pulled in via the -include at the bottom.
CFLAGS  := -O2 -std=c11 -Wall -Werror -MMD -MP -Isrc -Isrc/gen -Isrc/immix -I$(RT) -I$(JITRT) -I$(CRT)
# -fno-vectorize/-fno-slp-vectorize: a copy-and-patch stencil must be position-independent
# and relocation-free, but clang's auto-vectorizer emits .rodata vector constants (mask
# tables) the JIT can't relocate (jitterator rejects them). Keeping the SIMD lane loops
# SCALAR in the stencils is the "supported" tier — correct, both tiers; hardware-SIMD
# codegen (the "optimized" tier) is deferred. The interp (gen_interp.c) still vectorizes.
STENCIL_CFLAGS := -O2 -fno-stack-protector -fno-asynchronous-unwind-tables -fno-vectorize -fno-slp-vectorize -MMD -MP -Isrc -Isrc/gen -Isrc/immix -I$(RT) -I$(CRT)

GEN     := src/gen
B       := build
GC_OBJS := $(B)/immix_line_map.o $(B)/immix_block.o $(B)/immix_block_allocator.o \
           $(B)/immix_space.o $(B)/jav_gc.o
# The VM engine — the runtime, with NO owning reader/writer: the VM loads via the c-lite
# zero-copy reader, never the owning tree. The owning reader/writer are a separate group
# (water + the owning-reader tests link them; the c-lite load path test_load does not).
ENGINE_OBJS := $(B)/interp.o $(B)/jav_runtime.o $(B)/gen_interp.o $(B)/validate.o \
               $(B)/jav_subtype.o $(B)/jit_driver.o $(B)/bbq_arena.o $(B)/bbq_htree.o \
               $(GC_OBJS)
OWNING_OBJS := $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o
OBJS    := $(ENGINE_OBJS) $(OWNING_OBJS)
# The c-lite load path: the self-contained zero-copy reader + nav + its runtime,
# linked with $(ENGINE_OBJS) (NOT the owning reader) into the VM's overlay-driven
# load tests (test_load / test_skeleton / test_div).
CLITE_SRCS := src/jav_view_nav.c src/jav_module_index.c src/jav_module_validate.c src/jav_instance.c src/jav_extern.c src/jav_error.c $(GEN)/jav_view_reader.c $(RT)/bbq_lite.c src/jav_utf8.c


# The validated gates (interp == JIT differential + interp execution).
TESTS   := test_ops test_locals test_control test_brtable test_globals_mem test_probe \
           test_call test_host test_tiers test_trap_reason test_jit_holes \
           test_jit_numeric test_jit_native test_jit_libm test_jit_sweep \
           test_jit_control test_jit_brtable test_jit_trap test_jit_trunc \
           test_jit_trunc_sat test_const_expr test_multivalue test_typecheck \
           test_call_indirect test_reftypes test_table_ops test_br_on test_i31 test_gc_roots \
           test_struct test_struct_gc test_array test_array_gc test_refcast test_br_on_cast test_gc_ctors test_gc_packed test_gc_segments test_refmore test_bulk_mem test_subword_memops test_select test_call_ref test_simd_loadext test_simd_compute test_typemodel test_exceptions test_depth test_tailcall test_simd test_multimem test_memory64 test_carrier_addr_tag

.PHONY: all test clean gen water
all: $(OBJS) water

# ── code generation ────────────────────────────────────────────────
# opgen: one declarative spec -> interpreter handlers + stencils + JIT metadata.
gen: $(GEN)/gen_interp.c $(GEN)/jav_reader.c $(GEN)/jav_trap_reason.h
# opgen writes ALL of these from one invocation — a GROUPED target (&:) so make knows every
# one derives from wasm.def and regenerates them together. Without this, only gen_interp.c
# was a known output, so jav_stencils.c / runtime_api.h / jav_jit_meta.h changes after a
# wasm.def edit weren't tracked → stale stencil table + objects (the bug this fixes).
$(GEN)/gen_interp.c $(GEN)/gen_interp.h $(GEN)/jav_stencils.c $(GEN)/opcodes.h \
$(GEN)/opcode_tables.c $(GEN)/runtime_api.h $(GEN)/jav_jit_meta.h $(GEN)/jav_valtype.h \
$(GEN)/jav_type_meta.h $(GEN)/jav_jit_symbols.h &: spec/wasm.def $(OPGEN)
	$(OPGEN) -i spec/wasm.def -prefix jav -o $(GEN)

# Note: spec/instructions.toml (the §5.4 opcode table consumed by test_instr) is
# a committed artifact, generated once from the spec reference decoder via the
# offline tool tools/gen_instr_toml.py — deliberately NOT a build step.

# The execution-trap vocabulary. UNLIKE the toml itself this IS a build step: it
# joins instructions.toml's `traps` (the canonical §7.10 message text) with the
# `error:` guard names wasm.def declares, and fails the build if wasm.def names a
# reason the spec table doesn't carry. That check is the coverage ledger's first
# leg, so it has to run on every wasm.def edit, not offline.
$(B)/gen_trap_reasons: tools/gen_trap_reasons.c $(B)/toml_parser.o $(B)/toml_doc.o $(B)/bbq_arena.o | $(B)
	$(CC) -O2 -std=c11 -Wall -Werror $(TOML_INC) -Isrc/gen \
	  tools/gen_trap_reasons.c $(B)/toml_parser.o $(B)/toml_doc.o $(B)/bbq_arena.o -o $@
$(GEN)/jav_trap_reason.h: spec/instructions.toml spec/wasm.def $(B)/gen_trap_reasons
	$(B)/gen_trap_reasons spec/instructions.toml spec/wasm.def $(GEN)/jav_trap_reason.h

# bbqc: the module container grammar -> reader + writer + types (one source: wasm.bbq).
$(GEN)/jav_reader.c $(GEN)/jav_reader.h $(GEN)/jav_writer.c $(GEN)/jav_writer.h $(GEN)/jav_types.h: spec/wasm.bbq $(BBQC)
	$(BBQC) spec/wasm.bbq -backend c -prefix jav -templates $(BBQ_TEMPLATES) -o $(GEN)/

# bbqc c-lite: the zero-copy span-index reader — the VM's module-load read path
# (mmap + decode-on-access, no owning tree). A distinct -prefix so it coexists
# with the owning reader above; the runtime is $(RT)/bbq_lite.c.
$(GEN)/jav_view_reader.c $(GEN)/jav_view_reader.h: spec/wasm.bbq $(BBQC)
	$(BBQC) spec/wasm.bbq -backend c-lite -prefix jav_view -templates $(BBQ_TEMPLATES) -o $(GEN)/

# pegc: the §6 text grammar -> the .wat reader, building the SAME jav_module_t
# the binary reader produces (struct is the contract; oracle = struct-equivalence).
$(GEN)/wat_parser.c $(GEN)/wat_parser.h: spec/wat.peg $(PEGC)
	$(PEGC) spec/wat.peg -lang c -frames $(PEG_FRAMES) -prefix wat -o $(GEN)/

# jitterator: compiled stencils -> the copy-and-patch stencil table. opgen emits
# both the per-opcode stencils and the machine-boundary machinery (entry/halt/
# resync/trap) into jav_stencils.c, so jitterator runs on it directly.
$(GEN)/jav_stencil_table.h: $(GEN)/jav_stencils.c src/jav_frame.h $(GEN)/gen_interp.c $(GEN)/jav_trap_reason.h | $(B)
	$(CC) $(STENCIL_CFLAGS) -c $(GEN)/jav_stencils.c -o $(B)/jav_stencils.o
	$(JITTERATOR) $(B)/jav_stencils.o -o $(GEN)/jav_stencil_table.h

# ── objects ────────────────────────────────────────────────────────
$(B):
	mkdir -p $(B)
$(B)/interp.o:       src/interp.c       $(GEN)/gen_interp.c $(GEN)/jav_trap_reason.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/jav_runtime.o: src/jav_runtime.c $(GEN)/gen_interp.c $(GEN)/jav_trap_reason.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/validate.o:     src/validate.c     $(GEN)/jav_stencil_table.h $(GEN)/gen_interp.c $(GEN)/jav_trap_reason.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/gen_interp.o:   $(GEN)/gen_interp.c $(GEN)/jav_trap_reason.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/toml_parser.o:  src/gen/toml_parser.c | $(B)
	$(CC) -O2 -std=c11 -Wall $(TOML_INC) -c $< -o $@
$(B)/toml_doc.o:     src/toml/toml_doc.c | $(B)
	$(CC) -O2 -std=c11 -Wall $(TOML_INC) -c $< -o $@
$(B)/bbq_arena.o:    $(CRT)/bbq_arena.c | $(B)
	$(CC) -O2 -std=c11 -Wall $(TOML_INC) -c $< -o $@
$(B)/bbq_htree.o:    $(CRT)/bbq_htree.c | $(B)   # jit_driver's footer-pool dedup cache
	$(CC) -O2 -std=c11 -Wall $(TOML_INC) -c $< -o $@
$(B)/jav_reader.o:  $(GEN)/jav_reader.c | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/jav_writer.o:  $(GEN)/jav_writer.c | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/jav_utf8.o:    src/jav_utf8.c src/jav_utf8.h | $(B)   # §5.2.4 name validator, shared by all readers
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/jav_subtype.o: src/jav_subtype.c src/jav_subtype.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(B)/jit_driver.o:   src/jit_driver.c $(GEN)/jav_stencil_table.h $(GEN)/gen_interp.c | $(B)
	$(CC) $(CFLAGS) -c $< -o $@
$(GC_OBJS): $(B)/%.o: src/immix/%.c | $(B)
	$(CC) $(CFLAGS) -c $< -o $@

# ── the ported Immix GC engine (standalone: pure C, bbq_vec, no generated runtime) ──
IMMIX_SRC    := src/immix/immix_line_map.c src/immix/immix_block.c \
                src/immix/immix_block_allocator.c src/immix/immix_space.c src/immix/jav_gc.c
IMMIX_TESTS  := test_immix_line test_immix_block test_immix_pool test_immix_space test_gc test_gc_churn test_gc_evac_stress test_gc_los
IMMIX_CFLAGS := -O2 -std=c11 -Wall -Werror -Isrc/immix -I/home/dan/Source/BBQ/crt

# ── tests ──────────────────────────────────────────────────────────
test: $(OBJS) $(TOML_OBJS) $(GEN)/wat_parser.c $(GEN)/jav_view_reader.c
	@pass=0; fail=0; \
	$(CC) -O2 -std=c11 -Wall -Werror -Isrc test/test_subtype.c src/jav_subtype.c -o $(B)/test_subtype \
	  && $(B)/test_subtype >/dev/null 2>&1 && { echo "  PASS  test_subtype"; pass=$$((pass+1)); } \
	  || { echo "  FAIL  test_subtype"; fail=$$((fail+1)); }; \
	$(CC) -O2 -std=c11 -Wall -Werror -Isrc -Isrc/gen -Isrc/immix -I$(RT) -I$(CRT) test/test_storage.c -o $(B)/test_storage \
	  && $(B)/test_storage >/dev/null 2>&1 && { echo "  PASS  test_storage"; pass=$$((pass+1)); } \
	  || { echo "  FAIL  test_storage"; fail=$$((fail+1)); }; \
	$(CC) -O2 -std=c11 -Wall -Werror -I$(RT) -I$(CRT) test/test_leb.c -o $(B)/test_leb \
	  && $(B)/test_leb >/dev/null 2>&1 && { echo "  PASS  test_leb"; pass=$$((pass+1)); } \
	  || { echo "  FAIL  test_leb"; fail=$$((fail+1)); }; \
	for t in $(IMMIX_TESTS); do \
	  $(CC) $(IMMIX_CFLAGS) test/$$t.c $(IMMIX_SRC) -o $(B)/$$t || { echo "  $$t: BUILD FAIL"; fail=$$((fail+1)); continue; }; \
	  if $(B)/$$t >/dev/null 2>&1; then echo "  PASS  $$t"; pass=$$((pass+1)); \
	  else echo "  FAIL  $$t"; fail=$$((fail+1)); fi; \
	done; \
	for t in $(TESTS); do \
	  $(CC) $(CFLAGS) test/$$t.c $(OBJS) -lm -o $(B)/$$t || { echo "  $$t: BUILD FAIL"; fail=$$((fail+1)); continue; }; \
	  if ( cd test && ../$(B)/$$t >/dev/null 2>&1 ); then echo "  PASS  $$t"; pass=$$((pass+1)); \
	  else echo "  FAIL  $$t"; fail=$$((fail+1)); fi; \
	done; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_skeleton.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_skeleton && \
	  ( cd test && ../$(B)/test_skeleton add.wasm >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_skeleton"; pass=$$((pass+1)); } || { echo "  FAIL  test_skeleton"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude test/test_align.c $(CLITE_SRCS) $(ENGINE_OBJS) $(TOML_OBJS) -lm -o $(B)/test_align && \
	  ( cd test && ../$(B)/test_align >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_align"; pass=$$((pass+1)); } || { echo "  FAIL  test_align"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_div.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_div && \
	  ( cd test && ../$(B)/test_div >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_div"; pass=$$((pass+1)); } || { echo "  FAIL  test_div"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_instantiate.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_instantiate && \
	  ( cd test && ../$(B)/test_instantiate >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_instantiate"; pass=$$((pass+1)); } || { echo "  FAIL  test_instantiate"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_module_index.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_module_index && \
	  ( cd test && ../$(B)/test_module_index >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_module_index"; pass=$$((pass+1)); } || { echo "  FAIL  test_module_index"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_gc_funcref.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_gc_funcref && \
	  ( cd test && ../$(B)/test_gc_funcref >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_gc_funcref"; pass=$$((pass+1)); } || { echo "  FAIL  test_gc_funcref"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_addrtype_spec.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_addrtype_spec && \
	  ( cd test && ../$(B)/test_addrtype_spec >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_addrtype_spec"; pass=$$((pass+1)); } || { echo "  FAIL  test_addrtype_spec"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_gc_roots_real.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_gc_roots_real && \
	  ( cd test && ../$(B)/test_gc_roots_real >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_gc_roots_real"; pass=$$((pass+1)); } || { echo "  FAIL  test_gc_roots_real"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_gc_refforms.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_gc_refforms && \
	  ( cd test && ../$(B)/test_gc_refforms >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_gc_refforms"; pass=$$((pass+1)); } || { echo "  FAIL  test_gc_refforms"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_module_validate.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_module_validate && \
	  ( cd test && ../$(B)/test_module_validate >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_module_validate"; pass=$$((pass+1)); } || { echo "  FAIL  test_module_validate"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function test/test_gc_validation.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_gc_validation && \
	  ( cd test && ../$(B)/test_gc_validation >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_gc_validation"; pass=$$((pass+1)); } || { echo "  FAIL  test_gc_validation"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function \
	    test/test_load.c $(CLITE_SRCS) $(ENGINE_OBJS) -lm -o $(B)/test_load && \
	  ( cd test && ../$(B)/test_load add.wasm >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_load"; pass=$$((pass+1)); } || { echo "  FAIL  test_load"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) test/test_roundtrip.c $(OBJS) -lm -o $(B)/test_roundtrip && \
	  ( cd test && ../$(B)/test_roundtrip add.wasm >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_roundtrip"; pass=$$((pass+1)); } || { echo "  FAIL  test_roundtrip"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) test/test_func.c $(OBJS) -lm -o $(B)/test_func && \
	  ( cd test && ../$(B)/test_func add.wasm >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_func"; pass=$$((pass+1)); } || { echo "  FAIL  test_func"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) test/test_types.c $(OBJS) -lm -o $(B)/test_types && \
	  ( cd test && ../$(B)/test_types >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_types"; pass=$$((pass+1)); } || { echo "  FAIL  test_types"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) test/test_sections.c $(OBJS) -lm -o $(B)/test_sections && \
	  ( cd test && ../$(B)/test_sections >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_sections"; pass=$$((pass+1)); } || { echo "  FAIL  test_sections"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) test/test_malformed.c $(OBJS) -lm -o $(B)/test_malformed && \
	  ( cd test && ../$(B)/test_malformed >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_malformed"; pass=$$((pass+1)); } || { echo "  FAIL  test_malformed"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Iinclude -I$(PEGRT) test/test_instr.c $(OBJS) $(TOML_OBJS) -lm -o $(B)/test_instr && \
	  ( cd test && ../$(B)/test_instr >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_instr"; pass=$$((pass+1)); } || { echo "  FAIL  test_instr"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) test/test_wat.c src/wat_driver.c $(GEN)/wat_parser.c $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o $(B)/toml_parser.o $(B)/toml_doc.o $(B)/bbq_arena.o -lm -o $(B)/test_wat && \
	  ( cd test && ../$(B)/test_wat >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_wat"; pass=$$((pass+1)); } || { echo "  FAIL  test_wat"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) test/test_wast.c test/wast_exec.c src/wasm_capi.c $(GEN)/wat_parser.c \
	    src/jav_validate_module.c $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o $(B)/toml_parser.o $(B)/toml_doc.o $(B)/bbq_arena.o \
	    src/jav_load.c src/jav_view_nav.c src/jav_module_index.c src/jav_module_validate.c src/jav_instance.c src/jav_extern.c src/jav_error.c $(CRT)/bbq_htree.c $(GEN)/jav_view_reader.c $(RT)/bbq_lite.c $(B)/validate.o $(B)/jav_subtype.o $(B)/interp.o $(B)/jav_runtime.o $(B)/gen_interp.o $(B)/jit_driver.o $(GC_OBJS) -lm -o $(B)/test_wast && \
	  ( cd test && ../$(B)/test_wast ../../testsuite/*.wast regress_*.wast 2>/dev/null ) && \
	  { echo "  PASS  conformance"; pass=$$((pass+1)); } || { echo "  FAIL  conformance"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -Wno-unused-function -Iinclude -I$(PEGRT) test/test_water.c src/wat_driver.c $(GEN)/wat_parser.c $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o $(B)/toml_parser.o $(B)/toml_doc.o $(B)/bbq_arena.o -lm -o $(B)/test_water && \
	  ( cd test && ../$(B)/test_water >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_water"; pass=$$((pass+1)); } || { echo "  FAIL  test_water"; fail=$$((fail+1)); }; \
	$(MAKE) -s water && \
	  ( cd test && printf '(module (func (export "id") (param i32) (result i32) local.get 0))' \
	      | ../$(B)/water -i ../spec/instructions.toml - > ../$(B)/.water_smoke.wasm 2>/dev/null \
	      && ../$(B)/test_roundtrip ../$(B)/.water_smoke.wasm >/dev/null 2>&1 ) && \
	  { echo "  PASS  water (cli → round-trippable .wasm)"; pass=$$((pass+1)); } || { echo "  FAIL  water (cli → round-trippable .wasm)"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude examples/embed.c src/wasm_capi.c \
	    $(CLITE_SRCS) src/jav_load.c $(ENGINE_OBJS) -lm -o $(B)/embed && \
	  ( cd test && printf '(module (func (export "add") (param i32 i32) (result i32) local.get 0 local.get 1 i32.add))' \
	      | ../$(B)/water -i ../spec/instructions.toml - > ../$(B)/.embed.wasm 2>/dev/null \
	      && ../$(B)/embed ../$(B)/.embed.wasm >/dev/null 2>&1 ) && \
	  { echo "  PASS  embed (public wasm.h → instantiate + call)"; pass=$$((pass+1)); } || { echo "  FAIL  embed (public wasm.h → instantiate + call)"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) test/test_capi.c src/wasm_capi.c \
	    src/wat_driver.c $(GEN)/wat_parser.c $(B)/jav_reader.o $(B)/jav_writer.o $(TOML_OBJS) \
	    $(CLITE_SRCS) src/jav_load.c $(ENGINE_OBJS) -lm -o $(B)/test_capi && \
	  ( cd test && ../$(B)/test_capi >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_capi (host funcs + imports + accessors + reflection + ref values)"; pass=$$((pass+1)); } || { echo "  FAIL  test_capi (host funcs + imports + accessors + reflection + ref values)"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) test/test_capi_gc.c src/wasm_capi.c \
	    src/wat_driver.c $(GEN)/wat_parser.c $(B)/jav_reader.o $(B)/jav_writer.o $(TOML_OBJS) \
	    $(CLITE_SRCS) src/jav_load.c $(ENGINE_OBJS) -lm -o $(B)/test_capi_gc && \
	  ( cd test && ../$(B)/test_capi_gc >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_capi_gc (store roots under collection)"; pass=$$((pass+1)); } || { echo "  FAIL  test_capi_gc (store roots under collection)"; fail=$$((fail+1)); }; \
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) test/test_capi_jit.c src/wasm_capi.c \
	    src/wat_driver.c $(GEN)/wat_parser.c $(B)/jav_reader.o $(B)/jav_writer.o $(TOML_OBJS) \
	    $(CLITE_SRCS) src/jav_load.c $(ENGINE_OBJS) -lm -o $(B)/test_capi_jit && \
	  ( cd test && ../$(B)/test_capi_jit >/dev/null 2>&1 ) && \
	  { echo "  PASS  test_capi_jit (wasm_config tier selection: interp == JIT)"; pass=$$((pass+1)); } || { echo "  FAIL  test_capi_jit (wasm_config tier selection: interp == JIT)"; fail=$$((fail+1)); }; \
	echo "── $$pass passed, $$fail failed ──"; \
	[ $$fail -eq 0 ]

# The tier-selection gate on its own (the `test` recipe runs it too): interp == JIT through
# the public wasm.h, driven by the wasm_config_t embedder option.
.PHONY: test-capi-jit
test-capi-jit: $(OBJS) $(TOML_OBJS) | $(B)
	$(CC) $(CFLAGS) -Wno-unused-function -Iinclude -I$(PEGRT) test/test_capi_jit.c src/wasm_capi.c \
	    src/wat_driver.c $(GEN)/wat_parser.c $(B)/jav_reader.o $(B)/jav_writer.o $(TOML_OBJS) \
	    $(CLITE_SRCS) src/jav_load.c $(ENGINE_OBJS) -lm -o $(B)/test_capi_jit
	@cd test && ../$(B)/test_capi_jit

# water: the .wat -> .wasm assembler. Same reader/writer/toml link set as the wat
# tests; -D_POSIX_C_SOURCE for isatty/fileno under -std=c11.
water: $(B)/water
$(B)/water: src/water.c src/wat_driver.c src/wat_driver.h $(GEN)/wat_parser.c \
            $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o $(TOML_OBJS) $(B)/bbq_arena.o | $(B)
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -Wno-unused-function -Iinclude -I$(PEGRT) \
	  src/water.c src/wat_driver.c $(GEN)/wat_parser.c \
	  $(B)/jav_reader.o $(B)/jav_writer.o $(B)/jav_utf8.o $(TOML_OBJS) $(B)/bbq_arena.o -lm -o $(B)/water

clean:
	rm -rf $(B)

# The auto-generated per-object header dependencies (-MMD). Pulled in at the VERY BOTTOM,
# AFTER `all` and every real target — otherwise the first .d file's first target hijacks
# make's default goal (it was doing exactly that: bare `make` built only gen_interp.o).
# Make unions these with the explicit rules above, so an edit to any included header
# (including upstream BBQ headers outside this tree) rebuilds the objects that use it.
-include $(wildcard $(B)/*.d)
