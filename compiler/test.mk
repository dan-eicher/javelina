# test.mk — the compiler's test build, as real make targets.
#
# Included at the END of Makefile so every source-set variable it reads
# (SEMA_SRCS, DDCG_SRCS, CLICK_SRCS, EXEC_SRCS, VM_*) is already defined.
#
# What this replaces: one shell recipe that recompiled every library source
# once per test binary — twenty times through sema, the ddcg closure and the
# Click optimizer for a twenty-test run — and sent each suite's output to
# /dev/null, so a failure printed one line and discarded the evidence.
#
# Here each source becomes ONE object, shared by every test that links it, and
# each test is a real target with real prerequisites. Touch sir_optimizer.c and
# only the suites that link it relink. `make build/test_sema` builds exactly
# that. Suite output is kept in build/logs/<test>.log and the log of anything
# that fails is dumped in full.

OBJ  := $(B)/obj
LOGS := $(B)/logs

# ── objects ─────────────────────────────────────────────────────────────────
# In-tree sources mirror their path under build/obj; the BBQ crt lives outside
# the tree, so it gets its own prefix rather than an ../.. path.
$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-parentheses-equality -Iinclude -Itest -Idriver $(INCLUDES) $(VM_INCLUDES) -c $< -o $@

$(OBJ)/crt/%.o: $(CRT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(INCLUDES) -c $< -o $@

$(OBJ)/vm/%.o: $(VM)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(INCLUDES) $(VM_INCLUDES) -c $< -o $@

$(OBJ)/vmgen/%.o: $(VM)/src/gen/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(INCLUDES) $(VM_INCLUDES) -c $< -o $@

$(OBJ)/rt/%.o: $(RT)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Iinclude $(INCLUDES) $(VM_INCLUDES) -c $< -o $@

src2obj = $(patsubst %.c,$(OBJ)/%.o,$(1))

SEMA_OBJS  := $(call src2obj,$(SEMA_SRCS))
DDCG_OBJS  := $(call src2obj,$(DDCG_SRCS))
CLICK_OBJS := $(call src2obj,$(CLICK_SRCS))
PARSER_OBJ := $(OBJ)/$(GEN)/java_parser.o
MATCHER_OBJ:= $(OBJ)/$(GEN)/codegen_matcher.o
ARENA_OBJ  := $(OBJ)/crt/bbq_arena.o
CRT_OBJS   := $(ARENA_OBJ) $(OBJ)/crt/bbq_htree.o $(OBJ)/crt/bbq_hmap.o $(OBJ)/crt/bbq_buf.o

WASM_TYPES_OBJ := $(OBJ)/src/compiler/wasm_types.o
CODEGEN_STRUCTURED_OBJ := $(OBJ)/src/compiler/codegen_structured.o

# The VM side of test_exec / test_wasm_module. Engine objects are reused
# PREBUILT from the VM's own build (see the vm-objs rule below); the c-api and
# reader/writer are compiled here, as they always were.
VM_RW_OBJS   := $(OBJ)/vmgen/jav_reader.o $(OBJ)/vmgen/jav_writer.o \
                $(OBJ)/vm/jav_validate_module.o
VM_CAPI_OBJS := $(OBJ)/vm/wasm_capi.o $(OBJ)/vm/jav_load.o $(OBJ)/vm/jav_view_nav.o \
                $(OBJ)/vm/jav_module_index.o $(OBJ)/vm/jav_module_validate.o \
                $(OBJ)/vm/jav_instance.o $(OBJ)/vm/jav_extern.o $(OBJ)/vm/jav_error.o \
                $(OBJ)/vm/jav_utf8.o $(OBJ)/vmgen/jav_view_reader.o $(OBJ)/rt/bbq_lite.o

# The compiler's tests link the VM's prebuilt engine objects. Make that a real
# dependency instead of a comment telling you to remember: a stale engine object
# silently linked into a compiler test is a bug that costs an afternoon.
.PHONY: vm-objs
vm-objs:
	@$(MAKE) --no-print-directory -C $(VM) $(patsubst $(VM)/%,%,$(VM_ENGINE_OBJS))

# ── per-test link sets ──────────────────────────────────────────────────────
COMPILER_TESTS := test_parse test_jint test_jbound test_lattice test_sema test_sir test_gamma \
                  test_click_partition test_sir_copy test_emit_wasm \
                  test_codegen_wasm test_scope_sidecar test_codegen_structured \
                  test_control_audit test_wasm_module test_wasm_types \
                  test_codegen_object test_click_backend test_exec \
                  test_simd_ledger

OBJS_test_parse              := $(PARSER_OBJ) $(ARENA_OBJ)
OBJS_test_jint               :=   # header-only core (jint.h) — no object deps
OBJS_test_jbound             :=   # header-only core (jbound.h) — no object deps
OBJS_test_lattice            := $(SEMA_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_sema               := $(SEMA_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_sir                := $(SEMA_OBJS) $(DDCG_OBJS) $(CLICK_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_gamma              := $(CLICK_OBJS) $(SEMA_OBJS) $(DDCG_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_click_partition    := $(CLICK_OBJS) $(SEMA_OBJS) $(DDCG_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_sir_copy           := $(ARENA_OBJ)
OBJS_test_emit_wasm          :=
OBJS_test_codegen_wasm       := $(MATCHER_OBJ) $(WASM_TYPES_OBJ) $(SEMA_OBJS) $(CRT_OBJS)
OBJS_test_scope_sidecar      := $(SEMA_OBJS) $(DDCG_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_codegen_structured := $(CODEGEN_STRUCTURED_OBJ) $(WASM_TYPES_OBJ) $(MATCHER_OBJ) \
                                $(SEMA_OBJS) $(DDCG_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_control_audit      := $(OBJS_test_codegen_structured)
OBJS_test_wasm_module        := $(OBJ)/src/compiler/wasm_module.o $(WASM_TYPES_OBJ) $(CODEGEN_STRUCTURED_OBJ) \
                                $(MATCHER_OBJ) $(SEMA_OBJS) $(DDCG_OBJS) $(CLICK_OBJS) \
                                $(VM_RW_OBJS) $(PARSER_OBJ) \
                                $(OBJ)/crt/bbq_htree.o $(OBJ)/crt/bbq_hmap.o $(OBJ)/crt/bbq_buf.o \
                                $(VM_CAPI_OBJS) $(VM_ENGINE_OBJS)
OBJS_test_wasm_types         := $(WASM_TYPES_OBJ) $(SEMA_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_codegen_object     := $(OBJS_test_codegen_structured)
OBJS_test_click_backend      := $(CODEGEN_STRUCTURED_OBJ) $(WASM_TYPES_OBJ) $(MATCHER_OBJ) \
                                $(SEMA_OBJS) $(DDCG_OBJS) $(CLICK_OBJS) $(PARSER_OBJ) $(CRT_OBJS)
OBJS_test_exec               := $(call src2obj,$(EXEC_SRCS)) $(VM_RW_OBJS) $(VM_CAPI_OBJS) $(VM_ENGINE_OBJS)
# The simd ledger re-reads the spec toml with the generator's own reader pair.
OBJS_test_simd_ledger        := $(OBJ)/vmgen/toml_parser.o $(OBJ)/vm/toml/toml_doc.o $(ARENA_OBJ)

# test_exec and test_wasm_module reach the VM; everything else is compiler-only.
$(B)/test_exec $(B)/test_wasm_module: | vm-objs

# Test objects are built by a pattern rule, which make would treat as an
# intermediate and delete after linking — recompiling every suite's own .c on
# every run, which is most of what this file exists to stop.
.SECONDARY:

.SECONDEXPANSION:
$(B)/%: $(OBJ)/test/%.o $$(OBJS_$$*) | $(B)
	$(CC) $(CFLAGS) $^ -lm -o $@

# ── running ─────────────────────────────────────────────────────────────────
# One suite: build it, run it, keep the log. On failure the whole log is
# printed — never filtered, never discarded.
define run_suite
	@mkdir -p $(LOGS)
	@if ./$(B)/$(1) > $(LOGS)/$(1).log 2>&1; then \
	    echo "  PASS  $(1)"; \
	else \
	    echo "  FAIL  $(1)  (log below, also at $(LOGS)/$(1).log)"; \
	    sed 's/^/      | /' $(LOGS)/$(1).log; \
	    exit 1; \
	fi
endef

# Sanitizer build of the execution corpus: `make test-exec-asan`.
#
# This used to be a binary somebody built by hand and left in build/, which
# nothing could reproduce and nothing re-ran — so it rotted. It is a target now.
# Sources are compiled WITH the sanitizer; the VM engine objects are reused
# prebuilt (ASAN still instruments everything compiled here, which is where the
# compiler-side allocation lives). detect_leaks=0: the suite deliberately keeps
# process-lifetime storage — the shared prelude AST and its arena.
.PHONY: test-exec-asan
test-exec-asan: generate-parser generate-ddcg generate-codegen | vm-objs $(B)
	$(CC) -g -O0 -std=c11 -fsanitize=address -Wno-parentheses-equality \
	    -Iinclude -Itest -Idriver $(INCLUDES) $(VM_INCLUDES) \
	    test/test_exec.c $(EXEC_SRCS) $(VM_RW_SRCS) $(VM_CAPI_SRCS) $(VM_ENGINE_OBJS) \
	    -lm -o $(B)/test_exec.asan
	@ASAN_OPTIONS=detect_leaks=0 ./$(B)/test_exec.asan > $(LOGS)/test_exec.asan.log 2>&1 \
	  && { echo "  PASS  test_exec (ASAN)"; } \
	  || { echo "  FAIL  test_exec (ASAN)"; sed 's/^/      | /' $(LOGS)/test_exec.asan.log; exit 1; }

# `make test-one T=test_sema` — build and run exactly one suite.
.PHONY: test-one
test-one: $(B)/$(T)
	$(call run_suite,$(T))

# The dashed per-suite names (`make test-sema`, `make test-click-partition`)
# that predate this file still work, and are now incremental: each depends on
# its binary, so only what changed is rebuilt.
SUITE_ALIASES := $(subst _,-,$(COMPILER_TESTS))
.PHONY: $(SUITE_ALIASES)
$(SUITE_ALIASES):
	@$(MAKE) --no-print-directory test-one T=$(subst -,_,$@)

# The full gate. Builds every suite first (so a compile error is reported
# before any test runs), then runs them, continuing past failures so one run
# reports every broken suite rather than only the first.
.PHONY: test
test: generate-parser generate-ddcg generate-codegen $(addprefix $(B)/,$(COMPILER_TESTS))
	@mkdir -p $(LOGS); pass=0; fail=0; failed=""; \
	for t in $(COMPILER_TESTS); do \
	  if ./$(B)/$$t > $(LOGS)/$$t.log 2>&1; then \
	    echo "  PASS  $$t"; pass=$$((pass+1)); \
	  else \
	    echo "  FAIL  $$t"; fail=$$((fail+1)); failed="$$failed $$t"; \
	  fi; \
	done; \
	for t in $$failed; do \
	  echo ""; echo "── $$t ─────────────────────────────────────────"; \
	  sed 's/^/  | /' $(LOGS)/$$t.log; \
	done; \
	if JAVELINA_CLICK=1 ./$(B)/test_exec > $(LOGS)/test_exec_click.log 2>&1; then \
	  echo "  PASS  test_exec (Click ON)"; pass=$$((pass+1)); \
	else \
	  echo "  FAIL  test_exec (Click ON)"; fail=$$((fail+1)); \
	  echo ""; echo "── test_exec (Click ON) ─────────────────────────"; \
	  sed 's/^/  | /' $(LOGS)/test_exec_click.log | head -60; \
	fi; \
	if $(MAKE) --no-print-directory test-bench > $(LOGS)/test_bench.log 2>&1; then \
	  echo "  PASS  test-bench (quick matrix, checksum gate)"; pass=$$((pass+1)); \
	else \
	  echo "  FAIL  test-bench (quick matrix, checksum gate)"; fail=$$((fail+1)); \
	  echo ""; echo "── test-bench ───────────────────────────────────"; \
	  sed 's/^/  | /' $(LOGS)/test_bench.log | tail -40; \
	fi; \
	echo ""; echo "compiler tests: $$pass passed, $$fail failed"; \
	[ $$fail -eq 0 ]

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
