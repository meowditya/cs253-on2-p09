# Makefile — Project 9 (Team O(n^2)). Owner: Architect.
#
#   make              build every program and test (release flags) into build/release/bin
#   make test         build + run all correctness tests            (scripts/run_tests.sh)
#   make demo         full-scale simulator: heap vs AVL identical output, plus a traced oracle check
#   make bench        every benchmark, CSVs in results/raw, figures  (scripts/run_bench.sh)
#   make bench-quick  a few-minute smoke version of the benchmark matrix (not for the report)
#   make sanitize     tests rebuilt with AddressSanitizer + UBSan in build/sanitize
#   make all          build, test, demo, bench — strictly one after another
#   make clean
#
# BUILD=release|count|sanitize picks the flavour; each has its own directory, so objects never mix.
# "count" turns on the operation counters (-DEQ_COUNTING=1); never time a count build.

CXX       ?= g++
STD       := -std=c++17
WARN      := -Wall -Wextra -pedantic -Werror
INCLUDES  := -Iinclude -Isrc/structA -Isrc/structB -Ioracle -Iapp -Iadversary -Ibench
DEPFLAGS  := -MMD -MP

SAN       := -fsanitize=address,undefined
BUILD     ?= release
ifeq ($(BUILD),release)
  OPT := -O2 -DNDEBUG
else ifeq ($(BUILD),count)
  OPT := -O2 -DNDEBUG -DEQ_COUNTING=1
else ifeq ($(BUILD),sanitize)
  OPT := -O1 -g -fno-omit-frame-pointer $(SAN)
else
  $(error BUILD must be release, count or sanitize)
endif

OUT       := build/$(BUILD)
CXXFLAGS  := $(STD) $(WARN) $(OPT) $(INCLUDES) $(DEPFLAGS)
LDFLAGS   := $(if $(filter sanitize,$(BUILD)),$(SAN),)

# ---- shared modules ---------------------------------------------------------------------------
QUEUES    := src/structA/seq_table.cpp src/structA/lazy_heap.cpp src/structB/avl_queue.cpp \
             oracle/sorted_oracle.cpp app/queue_factory.cpp
CHECKS    := oracle/checkers.cpp oracle/replay_core.cpp
DIFF      := oracle/difftest_core.cpp

# ---- programs: name -> source files ------------------------------------------------------------
TOOLS     := sim replay difftest gen bench_hold bench_sweep
sim_SRC         := app/sim_main.cpp app/simulator.cpp $(QUEUES) $(CHECKS)
replay_SRC      := oracle/replay_main.cpp $(QUEUES) $(CHECKS)
difftest_SRC    := oracle/difftest_main.cpp $(DIFF) $(QUEUES) $(CHECKS)
gen_SRC         := adversary/gen.cpp $(DIFF) $(QUEUES) $(CHECKS)
bench_hold_SRC  := bench/hold.cpp $(QUEUES)
bench_sweep_SRC := bench/cancel_sweep.cpp $(QUEUES)

TESTS     := interface_contract_test test_opstream test_structA test_structB test_oracle \
             test_checkers_mutations test_sim test_fullscale
interface_contract_test_SRC := tests/interface_contract_test.cpp
test_opstream_SRC           := tests/test_opstream.cpp
test_structA_SRC            := tests/test_structA.cpp $(QUEUES) $(CHECKS)
test_structB_SRC            := tests/test_structB.cpp $(QUEUES) $(CHECKS)
test_oracle_SRC             := tests/test_oracle.cpp $(QUEUES) $(CHECKS)
test_checkers_mutations_SRC := tests/test_checkers_mutations.cpp $(DIFF) $(QUEUES) $(CHECKS)
test_sim_SRC                := tests/test_sim.cpp app/simulator.cpp $(QUEUES) $(CHECKS)
test_fullscale_SRC          := tests/test_fullscale.cpp $(QUEUES) $(CHECKS)

PROGRAMS  := $(TOOLS) $(TESTS)
BINS      := $(addprefix $(OUT)/bin/,$(PROGRAMS))

.PHONY: build test demo bench bench-quick sanitize pdfs all clean
build: $(BINS)

$(OUT)/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

define LINK_RULE
$(OUT)/bin/$(1): $$(patsubst %.cpp,$(OUT)/obj/%.o,$$($(1)_SRC))
	@mkdir -p $$(dir $$@)
	$$(CXX) $$^ $$(LDFLAGS) -o $$@
endef
$(foreach p,$(PROGRAMS),$(eval $(call LINK_RULE,$(p))))

test: build
	BIN=$(OUT)/bin scripts/run_tests.sh $(if $(filter sanitize,$(BUILD)),--sanitize,)

demo: build
	BIN=$(OUT)/bin scripts/demo.sh

bench: build
	$(MAKE) BUILD=count build
	BIN=$(OUT)/bin COUNT_BIN=build/count/bin scripts/run_bench.sh --full

pdfs:
	scripts/make_pdfs.sh

bench-quick: build
	$(MAKE) BUILD=count build
	BIN=$(OUT)/bin COUNT_BIN=build/count/bin scripts/run_bench.sh --quick

sanitize:
	$(MAKE) BUILD=sanitize test

# Sequential on purpose: tests, the demo and the benchmarks never run at the same time,
# even under `make -j`.
all:
	$(MAKE) build
	$(MAKE) test
	$(MAKE) demo
	$(MAKE) bench

clean:
	rm -rf build

-include $(shell find build -name '*.d' 2>/dev/null)
